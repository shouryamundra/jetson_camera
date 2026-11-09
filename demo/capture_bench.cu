// Measures the whole pipeline: capture, hand-off to a (simulated) detector,
// and recording. Camera grab thread + a detector thread pulling the latest
// frame + a Recorder fed by the camera callback; per-second report and a
// final percentile summary. The output format is kept stable so runs of
// different versions compare line by line.

#include <sys/resource.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "src/camera.h"
#include "src/config.h"
#include "src/recorder.h"

namespace {

namespace jc = jetson_camera;
using Clock = std::chrono::steady_clock;
using ull = unsigned long long;  // NOLINT(runtime/int): printf %llu.

std::int64_t RealtimeUs() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

double CpuSeconds() {
  rusage u{};
  getrusage(RUSAGE_SELF, &u);
  return static_cast<double>(u.ru_utime.tv_sec + u.ru_stime.tv_sec) +
         static_cast<double>(u.ru_utime.tv_usec + u.ru_stime.tv_usec) / 1e6;
}

// Thread-safe samples: per-window avg/max plus whole-run percentiles.
class Series {
 public:
  void Add(double v) {
    std::lock_guard lock(mutex_);
    all_.push_back(v);
    sum_ += v;
    max_ = std::max(max_, v);
    ++n_;
  }

  std::string Window() {
    std::lock_guard lock(mutex_);
    char s[48];
    std::snprintf(s, sizeof(s), "%.2f/%.2f", n_ ? sum_ / n_ : 0.0, max_);
    sum_ = max_ = 0;
    n_ = 0;
    return s;
  }

  std::string Summary() {
    std::lock_guard lock(mutex_);
    if (all_.empty()) return "n/a";
    std::vector<double> v = all_;
    std::sort(v.begin(), v.end());
    const auto q = [&](double p) {
      return v[static_cast<std::size_t>(p * static_cast<double>(v.size() - 1))];
    };
    double total = 0;
    for (double x : v) total += x;
    char s[128];
    std::snprintf(s, sizeof(s),
                  "avg %7.3f  p50 %7.3f  p99 %7.3f  max %7.3f  (n=%zu)",
                  total / static_cast<double>(v.size()), q(.5), q(.99),
                  v.back(), v.size());
    return s;
  }

 private:
  std::mutex mutex_;
  std::vector<double> all_;
  double sum_ = 0;
  double max_ = 0;
  int n_ = 0;
};

// Stand-in for inference: reads the tensor and spins for `cycles`.
__global__ void BusyKernel(const void* tensor, std::int64_t cycles, int* sink) {
  const std::int64_t start = clock64();
  int acc = 0;
  while (clock64() - start < cycles) {
    acc += static_cast<const unsigned char*>(tensor)[threadIdx.x];
  }
  if (acc == 42) *sink = acc;
}

}  // namespace

int main(int argc, char** argv) {
  std::string pipeline_path = "config/pipeline.ini";
  std::vector<std::string> overrides;
  double seconds = 30;
  double infer_ms = 15;
  const char* record_path = nullptr;
  for (int i = 1; i + 1 < argc; i += 2) {
    const std::string_view k = argv[i];
    const char* v = argv[i + 1];
    if (k == "--pipeline") {
      pipeline_path = v;
    } else if (k == "--set") {
      overrides.emplace_back(v);
    } else if (k == "--seconds") {
      seconds = std::atof(v);
    } else if (k == "--infer-ms") {
      infer_ms = std::atof(v);
    } else if (k == "--record") {
      record_path = v;
    } else {
      std::fprintf(stderr,
                   "usage: %s [--pipeline config/pipeline.ini] "
                   "[--set section.key=value]... [--seconds N]\n"
                   "          [--infer-ms MS] [--record out.mkv]\n",
                   argv[0]);
      return 2;
    }
  }

  try {
    const jc::PipelineConfig config =
        jc::LoadPipelineConfig(pipeline_path, overrides);
    const jc::CameraOptions& co = config.camera;
    const jc::RecorderOptions& ro = config.recorder;
    if (record_path != nullptr && !co.nv12) {
      throw std::runtime_error("recording needs output.nv12 = true");
    }
    std::optional<jc::Recorder> recorder;
    if (record_path != nullptr) {
      recorder.emplace(record_path, co.out_width, co.out_height, ro);
    }
    Series capture_ms, gpu_ms, exp_to_ready_ms, handoff_ms, pickup_age_ms,
        skipped_per_pickup, infer_wait_ms;
    jc::Camera camera(co, [&](const jc::FrameRef& f) {
      const jc::FrameInfo& i = f.info();
      capture_ms.Add(static_cast<double>(i.ready_us - i.arrival_us) / 1000.0);
      gpu_ms.Add(i.gpu_ms);
      exp_to_ready_ms.Add(
          static_cast<double>(i.ready_us - i.exposure_start_us) / 1000.0);
      if (recorder) recorder->Push(f);
    });
    // Let the grab thread apply its scheduling settings before reporting.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const std::string pin = co.cpu < 0        ? "no (cpu = -1)"
                            : camera.pinned() ? "cpu " + std::to_string(co.cpu)
                                              : "FAILED";
    std::printf(
        "config %s | sensor %.1f fps, exposure %.0f us, gain %.2f dB, "
        "readout+USB latency estimate %d us | recorder %d fps\n"
        "realtime grab thread: %s | grab thread pinned: %s | memory locked: "
        "%s | watchdog: %s\n",
        pipeline_path.c_str(), camera.sensor_fps(), camera.exposure_us(),
        camera.gain_db(), camera.latency_us(), ro.fps,
        camera.realtime() ? "yes" : "no (needs rtprio limit / CAP_SYS_NICE)",
        pin.c_str(),
        camera.memory_locked()
            ? "yes"
            : (co.lock_memory ? "FAILED (needs memlock limit)" : "off"),
        co.watchdog ? "on" : "off");

    // Simulated detector: takes the newest frame and runs GPU work for
    // infer_ms on a normal-priority stream in short kernels (like TensorRT
    // layers), holding the frame the whole time.
    std::atomic<bool> running{true};
    std::atomic<std::uint64_t> pickups{0};
    std::thread detector([&] {
      cudaStream_t s = nullptr;
      int* sink = nullptr;
      int clock_khz = 0;
      if (cudaStreamCreateWithPriority(&s, cudaStreamNonBlocking, 0) !=
              cudaSuccess ||
          cudaMalloc(&sink, sizeof(int)) != cudaSuccess ||
          cudaDeviceGetAttribute(&clock_khz, cudaDevAttrClockRate, 0) !=
              cudaSuccess) {
        std::fputs("detector: CUDA setup failed\n", stderr);
        return;
      }
      const std::int64_t chunk_cycles =
          std::int64_t{clock_khz} * 250 / 1000;  // 0.25 ms.
      std::uint64_t last = 0;
      while (running) {
        const auto t0 = Clock::now();
        auto frame = camera.Latest(last, std::chrono::milliseconds(200));
        if (!frame) continue;
        const std::int64_t now = RealtimeUs();
        const jc::FrameInfo& i = frame->info();
        infer_wait_ms.Add(
            std::chrono::duration<double, std::milli>(Clock::now() - t0)
                .count());
        handoff_ms.Add(static_cast<double>(now - i.ready_us) / 1000.0);
        pickup_age_ms.Add(static_cast<double>(now - i.ExposureMidUs()) /
                          1000.0);
        if (last != 0) {
          skipped_per_pickup.Add(static_cast<double>(i.seq - last - 1));
        }
        last = i.seq;
        ++pickups;
        const auto until =
            Clock::now() + std::chrono::duration<double, std::milli>(infer_ms);
        while (Clock::now() < until) {
          BusyKernel<<<64, 256, 0, s>>>(frame->buffers().tensor, chunk_cycles,
                                        sink);
          if (cudaStreamSynchronize(s) != cudaSuccess) break;
        }
      }
      cudaFree(sink);
      cudaStreamDestroy(s);
    });

    const auto start = Clock::now();
    double cpu0 = CpuSeconds();
    jc::CameraStats c0 = camera.stats();
    jc::RecorderStats r0 = recorder ? recorder->stats() : jc::RecorderStats{};
    jc::RecorderStats rlast{};
    std::uint64_t p0 = 0;
    std::printf("%-6s | %-26s | %-34s | %-38s | %s\n", "t",
                "capture fps/gaps/drops",
                "arrival->ready ms  gpu ms  exp->ready",
                "detector pickups/s  skip  handoff  age",
                "recorder fps drop enc-ms MB/s  cpu%");
    while (std::chrono::duration<double>(Clock::now() - start).count() <
           seconds) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      const jc::CameraStats c = camera.stats();
      const double cpu = CpuSeconds();
      const std::uint64_t p = pickups.load();
      char rec[96] = "-";
      if (recorder) {
        const jc::RecorderStats r = recorder->stats();
        const auto enc = r.encoded - r0.encoded;
        const std::uint64_t in_flight =
            static_cast<std::uint64_t>(ro.queue) + 3;
        std::snprintf(
            rec, sizeof(rec), "%4llu %4llu %6.2f %5.2f", static_cast<ull>(enc),
            static_cast<ull>(r.dropped(in_flight) - rlast.dropped(in_flight)),
            enc ? (r.latency_ms_sum - r0.latency_ms_sum) /
                      static_cast<double>(enc)
                : 0.0,
            static_cast<double>(r.bytes - r0.bytes) / 1e6);
        rlast = r;
        r0 = r;
      }
      std::printf(
          "%5.0fs | %6llu %4llu %4llu        | %-14s %-12s %-12s | %5llu "
          "%-10s %-10s %-10s | %s  %5.1f\n",
          std::chrono::duration<double>(Clock::now() - start).count(),
          static_cast<ull>(c.frames - c0.frames),
          static_cast<ull>(c.camera_gaps - c0.camera_gaps),
          static_cast<ull>(c.pool_drops - c0.pool_drops),
          capture_ms.Window().c_str(), gpu_ms.Window().c_str(),
          exp_to_ready_ms.Window().c_str(), static_cast<ull>(p - p0),
          skipped_per_pickup.Window().c_str(), handoff_ms.Window().c_str(),
          pickup_age_ms.Window().c_str(), rec, 100.0 * (cpu - cpu0));
      std::fflush(stdout);
      c0 = c;
      cpu0 = cpu;
      p0 = p;
    }
    running = false;
    detector.join();
    camera.Stop();
    const jc::CameraStats c = camera.stats();
    const double wall =
        std::chrono::duration<double>(Clock::now() - start).count();

    std::printf("\n===== SUMMARY (%.0f s, simulated inference %.1f ms) =====\n",
                wall, infer_ms);
    std::printf(
        "CAPTURE   frames %llu (%.1f fps)  camera gaps %llu  pool drops %llu  "
        "sdk errors %llu  copy fallbacks %llu\n",
        static_cast<ull>(c.frames), static_cast<double>(c.frames) / wall,
        static_cast<ull>(c.camera_gaps), static_cast<ull>(c.pool_drops),
        static_cast<ull>(c.errors), static_cast<ull>(c.copy_fallbacks));
    std::printf(
        "WATCHDOG  camera counter resets %llu  disconnects %llu  stalls %llu  "
        "reconnects %llu  usb resets %llu  connected at end: %s\n",
        static_cast<ull>(c.counter_resets), static_cast<ull>(c.disconnects),
        static_cast<ull>(c.stalls), static_cast<ull>(c.reconnects),
        static_cast<ull>(c.usb_resets), c.connected ? "yes" : "no");
    std::printf("  arrival -> frame ready (ms)        %s\n",
                capture_ms.Summary().c_str());
    std::printf("  GPU preprocess kernel (ms)         %s\n",
                gpu_ms.Summary().c_str());
    std::printf("  exposure start -> ready (ms)       %s\n",
                exp_to_ready_ms.Summary().c_str());
    std::printf("DETECTOR  pickups %llu (%.1f /s)\n",
                static_cast<ull>(pickups.load()),
                static_cast<double>(pickups.load()) / wall);
    std::printf("  wait for newer frame (ms)          %s\n",
                infer_wait_ms.Summary().c_str());
    std::printf("  frame ready -> picked up (ms)      %s\n",
                handoff_ms.Summary().c_str());
    std::printf("  frame age at pickup, from mid-exposure (ms) %s\n",
                pickup_age_ms.Summary().c_str());
    std::printf("  frames skipped between pickups     %s\n",
                skipped_per_pickup.Summary().c_str());
    if (recorder) {
      // Let in-flight frames finish before reading final counts.
      std::this_thread::sleep_for(std::chrono::milliseconds(300));
      const jc::RecorderStats r = recorder->stats();
      std::printf(
          "RECORDER  pushed %llu  encoded %llu  dropped (oldest) %llu  "
          "skipped for fps cap %llu  file %.1f MB (%.2f MB/s)\n",
          static_cast<ull>(r.pushed), static_cast<ull>(r.encoded),
          static_cast<ull>(r.dropped(0)), static_cast<ull>(r.skipped),
          static_cast<double>(r.bytes) / 1e6,
          static_cast<double>(r.bytes) / 1e6 / wall);
      std::printf(
          "  push -> encoded (ms)               avg %.2f  max %.2f\n",
          r.encoded ? r.latency_ms_sum / static_cast<double>(r.encoded) : 0.0,
          r.latency_ms_max);
      recorder.reset();  // EOS + finalise the file.
    }
  } catch (const std::exception& e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
  return 0;
}
