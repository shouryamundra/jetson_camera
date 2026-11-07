// Purpose: Run GpuCamera for a fixed time and report throughput and capture-to-GPU latency.
// Paradigm: Procedural driver over the GpuCamera start/read/stop interface.
// Role: End-to-end check of the USB-to-GPU pipeline on the target hardware.

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <vector>

#include "gpu_camera.hpp"

namespace
{
using Clock = std::chrono::steady_clock;

double milliseconds(const Clock::duration d)
{
  return std::chrono::duration<double, std::milli>(d).count();
}

void write_ppm(const io::GpuFrame & frame, const char * const path)
{
  std::vector<uchar4> bgra(
    static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height));
  io::cuda::check(
    cudaMemcpy(bgra.data(), frame.bgra, bgra.size() * sizeof(uchar4), cudaMemcpyDeviceToHost),
    "cudaMemcpy");

  std::FILE * const file = std::fopen(path, "wb");
  if (!file) {
    std::fprintf(stderr, "cannot write %s\n", path);
    return;
  }
  std::fprintf(file, "P6\n%d %d\n255\n", frame.width, frame.height);
  for (const uchar4 & p : bgra) {
    const unsigned char rgb[3] = {p.z, p.y, p.x};
    std::fwrite(rgb, 1, 3, file);
  }
  std::fclose(file);
  std::printf("snapshot written to %s\n", path);
}

}  // namespace

int main(const int argc, char ** const argv)
{
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s camera.ini [--seconds N] [--snapshot out.ppm]\n", argv[0]);
    return 2;
  }
  double seconds = 10;
  const char * snapshot = nullptr;
  for (int i = 2; i + 1 < argc; i += 2) {
    if (!std::strcmp(argv[i], "--seconds"))
      seconds = std::atof(argv[i + 1]);
    else if (!std::strcmp(argv[i], "--snapshot"))
      snapshot = argv[i + 1];
    else {
      std::fprintf(stderr, "unknown option %s\n", argv[i]);
      return 2;
    }
  }

  try {
    io::GpuCamera camera(argv[1]);
    camera.start();

    const auto end = Clock::now() + std::chrono::duration<double>(seconds);
    auto window = Clock::now();
    int frames = 0;
    std::uint32_t dropped = 0;
    double latency_sum = 0;
    double latency_max = 0;
    std::optional<std::uint32_t> last_index;
    int total = 0;

    while (Clock::now() < end) {
      const auto frame = camera.read();
      if (!frame) break;
      cudaStreamSynchronize(frame->stream);  // all GPU work for this frame is done
      const double latency = milliseconds(Clock::now() - frame->timestamp);

      if (last_index) dropped += frame->index - *last_index - 1;
      last_index = frame->index;
      latency_sum += latency;
      latency_max = std::max(latency_max, latency);
      ++frames;

      if (snapshot && ++total == 30) write_ppm(*frame, snapshot);  // after a short warm-up

      const double elapsed = milliseconds(Clock::now() - window) / 1000;
      if (elapsed >= 1) {
        std::printf(
          "%dx%d  %.1f fps  dropped %u  latency avg %.2f ms max %.2f ms\n", frame->width,
          frame->height, frames / elapsed, dropped, latency_sum / frames, latency_max);
        window = Clock::now();
        frames = 0;
        dropped = 0;
        latency_sum = latency_max = 0;
      }
    }
    camera.stop();
  } catch (const std::exception & e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
  return 0;
}
