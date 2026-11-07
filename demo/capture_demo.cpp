// Purpose: Run the camera for a fixed time, optionally recording, and report fps and latency.
// Paradigm: Procedural driver over Camera::read() and Recorder::record().
// Role: End-to-end check of the USB-to-GPU-to-disk pipeline on the Jetson.

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <optional>
#include <string_view>

#include "camera.hpp"
#include "recorder.hpp"

int main(const int argc, char ** const argv)
{
  using Clock = std::chrono::steady_clock;
  if (argc < 2) {
    std::fprintf(
      stderr, "usage: %s camera.ini [--seconds N] [--record out.mkv] [--fps N] [--bitrate BPS]\n",
      argv[0]);
    return 2;
  }
  double seconds = 10;
  const char * path = nullptr;
  io::RecorderOptions options;
  for (int i = 2; i + 1 < argc; i += 2) {
    const std::string_view key = argv[i];
    if (key == "--seconds")
      seconds = std::atof(argv[i + 1]);
    else if (key == "--record")
      path = argv[i + 1];
    else if (key == "--fps")
      options.fps = std::atoi(argv[i + 1]);
    else if (key == "--bitrate")
      options.bitrate = std::atoi(argv[i + 1]);
    else {
      std::fprintf(stderr, "unknown option %s\n", argv[i]);
      return 2;
    }
  }

  try {
    io::Camera camera(argv[1]);
    std::optional<io::Recorder> recorder;
    if (path) recorder.emplace(path, camera.width(), camera.height(), options);

    const auto end = Clock::now() + std::chrono::duration<double>(seconds);
    auto window = Clock::now();
    int frames = 0;
    double latency_max = 0;
    while (Clock::now() < end) {
      const auto frame = camera.read();
      if (!frame) {
        std::fputs("no frame within a second\n", stderr);
        continue;
      }
      // GPU work for the frame goes here, queued on frame->stream.
      io::cuda::check(cudaStreamSynchronize(frame->stream), "cudaStreamSynchronize");
      latency_max = std::max(
        latency_max,
        std::chrono::duration<double, std::milli>(Clock::now() - frame->timestamp).count());
      if (recorder) recorder->record(*frame);

      ++frames;
      const double elapsed = std::chrono::duration<double>(Clock::now() - window).count();
      if (elapsed >= 1) {
        std::printf(
          "%dx%d  %.1f fps  frame %u  gpu latency max %.2f ms\n", frame->width, frame->height,
          frames / elapsed, frame->index, latency_max);
        window = Clock::now();
        frames = 0;
        latency_max = 0;
      }
    }
  } catch (const std::exception & e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
  return 0;
}
