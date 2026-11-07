// Purpose: Declare a Hikrobot USB camera that delivers demosaiced frames in GPU-visible memory.
// Paradigm: RAII object over the MVS SDK handle; capture runs while the object lives.
// Role: Supplies timestamped BGRA frames for GPU work, with no CPU pixel copy.

#ifndef IO__CAMERA_HPP
#define IO__CAMERA_HPP

#include <cuda_runtime.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "bayer.hpp"
#include "cuda.hpp"

namespace io
{
struct GpuFrame
{
  const uchar4 * bgra;  // CPU- and GPU-readable, tightly packed; overwritten by the next read()
  int width;
  int height;
  cudaStream_t stream;  // queue GPU work here; the next read() waits for all of it
  std::chrono::steady_clock::time_point timestamp;  // when read() received the frame
  std::uint32_t index;  // the camera's frame counter; a gap means skipped frames
};

class Camera
{
public:
  // Opens the first USB camera, applies the MVS feature file and starts capturing.
  // Throws std::runtime_error if the camera or its pixel format is unusable.
  explicit Camera(const std::string & ini_path);
  ~Camera();  // waits for queued GPU work, then stops and closes the camera

  Camera(const Camera &) = delete;
  Camera & operator=(const Camera &) = delete;

  [[nodiscard]] int width() const noexcept { return width_; }
  [[nodiscard]] int height() const noexcept { return height_; }

  // Waits for the previous frame's GPU work, takes the frame the SDK holds (at most one frame
  // old) and queues its demosaic on frame.stream. Work on any other stream must finish before
  // calling read() again. Returns nothing if no frame arrives within a second.
  [[nodiscard]] std::optional<GpuFrame> read();

private:
  struct Close
  {
    void operator()(void * handle) const noexcept;
  };

  std::unique_ptr<void, Close> handle_;
  int width_ = 0;
  int height_ = 0;
  unsigned payload_ = 0;  // bytes per raw frame
  Bayer bayer_{};
  cuda::Pinned<std::uint8_t> raw_;
  cuda::Pinned<uchar4> bgra_;
  cuda::Stream stream_;
};

}  // namespace io

#endif  // IO__CAMERA_HPP
