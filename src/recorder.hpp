// Purpose: Declare a recorder that hardware-encodes frames to an H.265 Matroska file.
// Paradigm: RAII-owned GStreamer pipeline; each recorded frame is copied in and owned by it.
// Role: Stores camera video to disk on the encoder's threads, like Tongji's Recorder.

#ifndef IO__RECORDER_HPP
#define IO__RECORDER_HPP

#include <gst/gst.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>

#include "camera.hpp"

namespace io
{
struct RecorderOptions
{
  int fps = 30;             // frames arriving faster than this are skipped
  int bitrate = 8'000'000;  // target bits per second
};

class Recorder
{
public:
  // Starts the encoder writing `width` x `height` frames to `path`.
  // Throws std::runtime_error if the pipeline cannot be built or started.
  Recorder(const std::string & path, int width, int height, RecorderOptions options = {});
  ~Recorder();  // finishes the file, waiting a bounded time for the encoder to drain

  Recorder(const Recorder &) = delete;
  Recorder & operator=(const Recorder &) = delete;

  // Waits for the frame's stream, copies the frame and queues it for encoding. Skips it if it
  // comes too soon after the last one or the encoder is three frames behind.
  void record(const GpuFrame & frame);

private:
  struct Stop
  {
    void operator()(GstElement * pipeline) const noexcept;
  };
  struct Unref
  {
    void operator()(gpointer object) const noexcept { gst_object_unref(object); }
  };

  std::unique_ptr<GstElement, Stop> pipeline_;
  std::unique_ptr<GstElement, Unref> source_;  // the appsrc inside pipeline_
  std::chrono::nanoseconds min_gap_;
  std::optional<std::chrono::steady_clock::time_point> first_;
  std::chrono::steady_clock::time_point last_{};
};

}  // namespace io

#endif  // IO__RECORDER_HPP
