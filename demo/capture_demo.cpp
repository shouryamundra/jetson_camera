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
void write_ppm(const io::GpuFrame & frame, const char * path)
{
  std::vector<uchar4> bgra(size_t(frame.width) * frame.height);
  cudaMemcpy(bgra.data(), frame.bgra, bgra.size() * sizeof(uchar4), cudaMemcpyDeviceToHost);

  std::FILE * file = std::fopen(path, "wb");
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

int main(int argc, char ** argv)
{
  if (argc < 2) {
    std::fprintf(
      stderr, "usage: %s camera.ini [--seconds N] [--roi x,y,w,h] [--snapshot out.ppm]\n", argv[0]);
    return 2;
  }
  double seconds = 10;
  io::Roi roi;
  const char * snapshot = nullptr;
  for (int i = 2; i + 1 < argc; i += 2) {
    if (!std::strcmp(argv[i], "--seconds"))
      seconds = std::atof(argv[i + 1]);
    else if (!std::strcmp(argv[i], "--roi"))
      std::sscanf(argv[i + 1], "%d,%d,%d,%d", &roi.x, &roi.y, &roi.width, &roi.height);
    else if (!std::strcmp(argv[i], "--snapshot"))
      snapshot = argv[i + 1];
    else {
      std::fprintf(stderr, "unknown option %s\n", argv[i]);
      return 2;
    }
  }

  using Clock = std::chrono::steady_clock;
  const auto ms = [](Clock::duration d) {
    return std::chrono::duration<double, std::milli>(d).count();
  };

  try {
    io::GpuCamera camera(argv[1], roi);
    camera.start();

    const auto end = Clock::now() + std::chrono::duration<double>(seconds);
    auto window = Clock::now();
    int frames = 0;
    unsigned dropped = 0;
    double latency_sum = 0, latency_max = 0;
    bool first = true;
    uint32_t last_index = 0;
    uint64_t total = 0;

    io::GpuFrame frame;
    while (Clock::now() < end && camera.read(frame)) {
      cudaStreamSynchronize(frame.stream);  // all GPU work for this frame is done
      const double latency = ms(Clock::now() - frame.timestamp);

      if (!first) dropped += frame.index - last_index - 1;
      first = false;
      last_index = frame.index;
      latency_sum += latency;
      latency_max = std::max(latency_max, latency);
      ++frames;

      if (snapshot && ++total == 30) write_ppm(frame, snapshot);  // after a short warm-up

      const double elapsed = ms(Clock::now() - window) / 1000;
      if (elapsed >= 1) {
        std::printf(
          "%dx%d  %.1f fps  dropped %u  latency avg %.2f ms max %.2f ms\n", frame.width,
          frame.height, frames / elapsed, dropped, latency_sum / frames, latency_max);
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
