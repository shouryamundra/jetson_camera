// Hardware-encoded recording of NV12 frames to H.265 Matroska.
//
// appsrc wraps each frame's pinned NV12 plane (zero copy; GStreamer releases
// the FrameRef); leaky-downstream drops the oldest queued frame; pad probes
// measure the encoder. splitmuxsink rotates segments and deletes the oldest;
// a monitor thread checks free space once a second.

#include "src/recorder.h"

#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <sys/statvfs.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cinttypes>
#include <condition_variable>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>

namespace jetson_camera {
namespace {

constexpr std::size_t kMaxPending = 256;
constexpr std::uint64_t kResumePercent = 120;  // Hysteresis: resume at 120%.

std::int64_t SteadyUs() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

// NV12 from system memory -> VIC copies it into NVMM (no colour conversion)
// -> NVENC H.265, a keyframe (with SPS/PPS) every second so every segment is
// independently playable.
std::string Describe(int width, int height, const RecorderOptions& o) {
  const int rate = o.fps > 0 ? o.fps : 240;
  const std::string sink =
      o.segment_s > 0
          ? "splitmuxsink name=sink muxer-factory=matroskamux "
            "async-finalize=true send-keyframe-requests=true max-size-time=" +
                std::to_string(std::int64_t{o.segment_s} * 1'000'000'000) +
                " max-files=" + std::to_string(o.max_files)
          : "matroskamux ! filesink name=sink";
  return "appsrc name=source is-live=true format=time block=false "
         "leaky-type=downstream max-buffers=" +
         std::to_string(o.queue) +
         " caps=video/x-raw,format=NV12,colorimetry=bt709,width=" +
         std::to_string(width) + ",height=" + std::to_string(height) +
         ",framerate=" + std::to_string(rate) +
         "/1 ! nvvidconv ! video/x-raw(memory:NVMM),format=NV12 ! "
         "nvv4l2h265enc insert-sps-pps=true iframeinterval=" +
         std::to_string(rate) + " idrinterval=" + std::to_string(rate) +
         " bitrate=" + std::to_string(o.bitrate) +
         " ! h265parse name=parse ! queue name=written ! " + sink;
}

// "out.mkv" -> "out_%05d.mkv"; a path that already has '%' is kept.
std::string SegmentPattern(const std::string& path) {
  if (path.find('%') != std::string::npos) return path;
  const std::filesystem::path p(path);
  return (p.parent_path() /
          (p.stem().string() + "_%05d" + p.extension().string()))
      .string();
}

// Free bytes for unprivileged users on the filesystem holding `dir`.
std::uint64_t FreeBytes(const std::string& dir) {
  struct statvfs s{};
  if (statvfs(dir.c_str(), &s) != 0)
    return UINT64_MAX;  // Unknown: don't block.
  return static_cast<std::uint64_t>(s.f_bavail) * s.f_frsize;
}

void ReleaseFrame(gpointer frame) { delete static_cast<FrameRef*>(frame); }

struct Unref {
  void operator()(gpointer object) const noexcept { gst_object_unref(object); }
};
struct StopPipeline {
  void operator()(GstElement* pipeline) const noexcept {
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
  }
};

}  // namespace

struct Recorder::Impl {
  RecorderOptions options;
  std::size_t frame_bytes = 0;
  std::unique_ptr<GstElement, StopPipeline> pipeline;
  std::unique_ptr<GstElement, Unref> source;
  // Producer-only pacing state.
  std::optional<std::int64_t> first_us;
  std::int64_t next_due_us = 0;

  mutable std::mutex mutex;  // Guards stats and pending (GStreamer threads).
  RecorderStats stats;
  std::unordered_map<std::uint64_t, std::int64_t> pending;  // PTS -> push us.

  // Free-space monitor: the hot path only reads disk_low.
  std::atomic<bool> disk_low{false};
  std::atomic<std::uint64_t> disk_skipped{0};
  std::mutex monitor_mutex;
  std::condition_variable monitor_cv;
  bool monitor_stop = false;
  std::thread monitor;

  void MonitorDisk(const std::string& dir) {
    const std::uint64_t min_free =
        static_cast<std::uint64_t>(options.min_free_mb) << 20;
    std::unique_lock lock(monitor_mutex);
    do {  // Check immediately, then once a second until stopped.
      const std::uint64_t free = FreeBytes(dir);
      if (!disk_low && free < min_free) {
        disk_low = true;
        std::fprintf(stderr, "recorder: %" PRIu64 " MB free, pausing\n",
                     free >> 20);
      } else if (disk_low && free > min_free / 100 * kResumePercent) {
        disk_low = false;
        std::fputs("recorder: disk space recovered, resuming\n", stderr);
      }
    } while (!monitor_cv.wait_for(lock, std::chrono::seconds(1),
                                  [this] { return monitor_stop; }));
  }

  static GstPadProbeReturn OnEncoded(GstPad*, GstPadProbeInfo* info,
                                     gpointer self_ptr) {
    auto* self = static_cast<Impl*>(self_ptr);
    const GstClockTime pts = GST_BUFFER_PTS(GST_PAD_PROBE_INFO_BUFFER(info));
    const std::int64_t now = SteadyUs();
    std::lock_guard lock(self->mutex);
    ++self->stats.encoded;
    if (const auto it = self->pending.find(pts); it != self->pending.end()) {
      const double ms = static_cast<double>(now - it->second) / 1000.0;
      self->stats.latency_ms_sum += ms;
      self->stats.latency_ms_max = std::max(self->stats.latency_ms_max, ms);
      self->pending.erase(it);
    }
    return GST_PAD_PROBE_OK;
  }

  static GstPadProbeReturn OnWritten(GstPad*, GstPadProbeInfo* info,
                                     gpointer self_ptr) {
    auto* self = static_cast<Impl*>(self_ptr);
    std::lock_guard lock(self->mutex);
    self->stats.bytes += gst_buffer_get_size(GST_PAD_PROBE_INFO_BUFFER(info));
    return GST_PAD_PROBE_OK;
  }
};

Recorder::Recorder(const std::string& path, int width, int height,
                   RecorderOptions options)
    : impl_(std::make_unique<Impl>()) {
  Impl& m = *impl_;
  m.options = options;
  m.frame_bytes = static_cast<std::size_t>(width) *
                  static_cast<std::size_t>(height) * 3 / 2;
  m.pending.reserve(kMaxPending);
  gst_init(nullptr, nullptr);
  GError* error = nullptr;
  m.pipeline.reset(
      gst_parse_launch(Describe(width, height, options).c_str(), &error));
  if (error != nullptr) {
    const std::string message = error->message;
    g_error_free(error);
    throw std::runtime_error("encoder pipeline: " + message);
  }
  const auto find = [&m](const char* name) {
    return std::unique_ptr<GstElement, Unref>(
        gst_bin_get_by_name(GST_BIN(m.pipeline.get()), name));
  };
  m.source = find("source");
  const std::string location =
      options.segment_s > 0 ? SegmentPattern(path) : path;
  g_object_set(find("sink").get(), "location", location.c_str(), nullptr);

  const std::unique_ptr<GstPad, Unref> encoded(
      gst_element_get_static_pad(find("parse").get(), "src"));
  gst_pad_add_probe(encoded.get(), GST_PAD_PROBE_TYPE_BUFFER, &Impl::OnEncoded,
                    &m, nullptr);
  const std::unique_ptr<GstPad, Unref> written(
      gst_element_get_static_pad(find("written").get(), "src"));
  gst_pad_add_probe(written.get(), GST_PAD_PROBE_TYPE_BUFFER, &Impl::OnWritten,
                    &m, nullptr);

  if (gst_element_set_state(m.pipeline.get(), GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE) {
    throw std::runtime_error("cannot start the encoder pipeline");
  }
  if (options.min_free_mb > 0) {
    std::string dir = std::filesystem::path(location).parent_path().string();
    if (dir.empty()) dir = ".";
    m.monitor = std::thread([&m, dir] { m.MonitorDisk(dir); });
  }
}

Recorder::~Recorder() {
  if (impl_->monitor.joinable()) {
    {
      std::lock_guard lock(impl_->monitor_mutex);
      impl_->monitor_stop = true;
    }
    impl_->monitor_cv.notify_all();
    impl_->monitor.join();
  }
  gst_app_src_end_of_stream(GST_APP_SRC(impl_->source.get()));
  const std::unique_ptr<GstBus, Unref> bus(
      gst_element_get_bus(impl_->pipeline.get()));
  GstMessage* const done = gst_bus_timed_pop_filtered(
      bus.get(),
      static_cast<GstClockTime>(impl_->options.finish_timeout_ms) * GST_MSECOND,
      static_cast<GstMessageType>(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
  if (done != nullptr) {
    gst_message_unref(done);
  } else {
    std::fputs("recorder: encoder did not finish in time\n", stderr);
  }
  // The pipeline goes to NULL next, releasing every frame GStreamer holds.
}

void Recorder::Push(const FrameRef& frame) {
  Impl& m = *impl_;
  if (m.disk_low.load(std::memory_order_relaxed)) {
    m.disk_skipped.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  const std::int64_t t = frame.info().exposure_start_us;
  // Deadline pacing: record the first frame at or after each 1/fps tick (a
  // quarter period early is allowed), so the average rate matches `fps`.
  if (m.options.fps > 0) {
    const std::int64_t period = 1'000'000 / m.options.fps;
    if (m.first_us && t + period / 4 < m.next_due_us) {
      std::lock_guard lock(m.mutex);
      ++m.stats.skipped;
      return;
    }
    m.next_due_us = (m.first_us && t - m.next_due_us < period)
                        ? m.next_due_us + period
                        : t + period;
  }
  if (!m.first_us) m.first_us = t;

  // Wrap the frame's NV12 plane; GStreamer owns a FrameRef until it is done.
  auto* owner = new FrameRef(frame);
  GstBuffer* const buffer = gst_buffer_new_wrapped_full(
      GST_MEMORY_FLAG_READONLY, frame.buffers().nv12, m.frame_bytes, 0,
      m.frame_bytes, owner, ReleaseFrame);
  const auto pts = static_cast<std::uint64_t>(t - *m.first_us) * 1000;
  GST_BUFFER_PTS(buffer) = pts;
  {
    std::lock_guard lock(m.mutex);
    ++m.stats.pushed;
    if (m.pending.size() >= kMaxPending) m.pending.clear();  // Bounded.
    m.pending[pts] = SteadyUs();
  }
  gst_app_src_push_buffer(GST_APP_SRC(m.source.get()), buffer);
}

RecorderStats Recorder::stats() const {
  std::lock_guard lock(impl_->mutex);
  RecorderStats s = impl_->stats;
  s.disk_skipped = impl_->disk_skipped;
  s.disk_low = impl_->disk_low;
  return s;
}

}  // namespace jetson_camera
