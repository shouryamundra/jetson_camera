// Purpose: Declare a Hikrobot USB camera that delivers demosaiced frames in GPU memory.
// Paradigm: RAII-owned pipeline with a capture thread and a lock-free frame handoff.
// Role: Supplies timestamped BGRA frames that GPU work can consume without any CPU copy.

#ifndef IO__GPU_CAMERA_HPP
#define IO__GPU_CAMERA_HPP

#include <cuda_runtime.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>

#include "bayer.hpp"
#include "cuda_raii.hpp"
#include "frame_mailbox.hpp"
#include "mvs_device.hpp"

namespace io
{
struct GpuFrame
{
  const uchar4 * bgra;  // device pointer, tightly packed, valid until the next read()
  int width;
  int height;
  cudaStream_t stream;  // enqueue GPU work on this stream; the next read() waits for it
  std::chrono::steady_clock::time_point timestamp;  // when the frame finished arriving over USB
  std::uint32_t index;  // the camera's frame counter; a gap means dropped frames
};

class GpuCamera
{
public:
  // Opens the camera, applies the MVS feature file at `ini_path` and allocates every buffer.
  // Throws std::runtime_error if the camera or its pixel format is unusable.
  explicit GpuCamera(const std::string & ini_path);

  // Stops capture and waits for queued GPU work, so no kernel outlives the buffers it reads.
  ~GpuCamera();

  GpuCamera(const GpuCamera &) = delete;
  GpuCamera & operator=(const GpuCamera &) = delete;

  [[nodiscard]] int width() const noexcept { return static_cast<int>(geometry_.width); }
  [[nodiscard]] int height() const noexcept { return static_cast<int>(geometry_.height); }

  void start();
  void stop();

  // Blocks until a frame newer than the previous one arrives and queues its conversion on the
  // returned stream. GPU work queued on that stream afterwards runs after the conversion, and the
  // next read() waits for it. Returns nothing once stopped. Call only after start().
  [[nodiscard]] std::optional<GpuFrame> read();

private:
  struct Slot
  {
    cuda::PinnedBuffer raw;  // written by the SDK, read by the GPU
    std::chrono::steady_clock::time_point timestamp{};
    std::uint32_t index = 0;
  };

  [[nodiscard]] std::unique_ptr<MvsDevice> open() const;
  void capture(const std::stop_token & stop);
  void recover(const std::stop_token & stop);

  // Members are listed in construction order; destruction runs in reverse, so the capture thread
  // is joined before the buffers it writes to are released.
  cuda::SpinScheduling spin_scheduling_;  // must come before the first CUDA allocation
  std::string ini_path_;
  std::unique_ptr<MvsDevice> device_;  // replaced when the camera is reopened
  MvsDevice::Geometry geometry_;
  Bayer bayer_;
  FrameMailbox<Slot> mailbox_;
  cuda::DeviceBuffer<uchar4> output_;
  cuda::Stream stream_;
  std::jthread thread_;
};

}  // namespace io

#endif  // IO__GPU_CAMERA_HPP
