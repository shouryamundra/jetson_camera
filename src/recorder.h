// Hardware-encodes camera frames to an H.265 Matroska file.
//
// Frames enter by reference (no pixel copy); the input queue drops the OLDEST
// frame when the encoder falls behind, so recording never slows capture.
// GStreamer stays out of this header (pimpl), so it also compiles under nvcc.

#ifndef JETSON_CAMERA_SRC_RECORDER_H_
#define JETSON_CAMERA_SRC_RECORDER_H_

#include <cstdint>
#include <memory>
#include <string>

#include "src/frame.h"

namespace jetson_camera {

// Defaults match config/pipeline.ini.
struct RecorderOptions {
  int fps = 30;             // Deadline-paced recording rate; 0 = every frame.
  int bitrate = 8'000'000;  // Target bits per second.
  int queue = 4;            // Frames queued before the oldest is dropped.
  int finish_timeout_ms = 5000;  // Shutdown wait for the encoder to finish.
};

struct RecorderStats {
  std::uint64_t pushed = 0;   // Handed to the pipeline.
  std::uint64_t skipped = 0;  // Skipped by the fps limit.
  std::uint64_t encoded = 0;  // Left the encoder.
  std::uint64_t bytes = 0;    // Written to the file.
  double latency_ms_sum = 0;  // Push -> encoded, summed over `encoded`.
  double latency_ms_max = 0;

  // Pushed but neither encoded nor still in flight: dropped as oldest.
  [[nodiscard]] std::uint64_t dropped(std::uint64_t in_flight) const {
    return pushed > encoded + in_flight ? pushed - encoded - in_flight : 0;
  }
};

class Recorder {
 public:
  // Starts the encoder writing width x height NV12 frames to `path`; throws
  // std::runtime_error if the pipeline cannot start.
  Recorder(const std::string& path, int width, int height,
           RecorderOptions options = {});
  ~Recorder();  // Finishes the file (bounded wait for the encoder).
  Recorder(const Recorder&) = delete;
  Recorder& operator=(const Recorder&) = delete;

  // Single producer, non-blocking. Keeps a reference to the frame until
  // GStreamer is done with it.
  void Push(const FrameRef& frame);
  [[nodiscard]] RecorderStats stats() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace jetson_camera

#endif  // JETSON_CAMERA_SRC_RECORDER_H_
