// Purpose: Declare a Hikrobot USB camera that delivers demosaiced frames in GPU memory.
// Paradigm: Object-oriented SDK adapter with a capture thread and a lock-free frame handoff.
// Role: Supplies timestamped BGRA frames that GPU work can consume without any CPU copy.

#ifndef IO__GPU_CAMERA_HPP
#define IO__GPU_CAMERA_HPP

#include <cuda_runtime.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

#include "bayer.hpp"
#include "demosaic.hpp"
#include "frame_mailbox.hpp"

namespace io
{
struct GpuFrame
{
  const uchar4 * bgra;  // device pointer, valid until the next read()
  int width;
  int height;
  size_t pitch;         // bytes between rows
  cudaStream_t stream;  // enqueue GPU work on this stream
  std::chrono::steady_clock::time_point timestamp;  // when the frame finished arriving over USB
  uint32_t index;  // camera frame counter; a gap means dropped frames
};

class GpuCamera
{
public:
  // Opens the camera, applies the settings saved in `ini_path` (MVS feature file) and allocates
  // all buffers. Throws std::runtime_error if the camera or the pixel format is unusable and
  // std::invalid_argument if `roi` does not fit the frame.
  GpuCamera(const std::string & ini_path, Roi roi = {});
  ~GpuCamera();
  GpuCamera(const GpuCamera &) = delete;
  GpuCamera & operator=(const GpuCamera &) = delete;

  void start();
  void stop();

  // Blocks until a frame newer than the previous one arrives and queues its conversion on
  // `frame.stream`. Work queued on that stream after this call runs after the conversion. The
  // previous frame's work is waited for here. Returns false after stop(). Call only after start().
  bool read(GpuFrame & frame);

private:
  struct Slot
  {
    uint8_t * host = nullptr;          // pinned, mapped raw frame written by the SDK
    const uint8_t * device = nullptr;  // the same memory as seen by the GPU
    std::chrono::steady_clock::time_point timestamp;
    uint32_t index = 0;
  };

  struct Geometry
  {
    unsigned width = 0;
    unsigned height = 0;
    unsigned payload = 0;
    unsigned pixel_type = 0;
    bool operator==(const Geometry & o) const
    {
      return width == o.width && height == o.height && payload == o.payload &&
             pixel_type == o.pixel_type;
    }
  };

  bool open_device(Geometry & geometry);
  void close_device();
  void capture_loop();
  void recover();

  std::string ini_path_;
  Roi roi_;
  Bayer bayer_;
  Geometry geometry_;

  void * handle_ = nullptr;
  cudaStream_t stream_ = nullptr;
  uchar4 * output_ = nullptr;

  FrameMailbox<Slot> mailbox_;
  std::thread capture_thread_;
  std::atomic<bool> quit_{false};
};

}  // namespace io

#endif  // IO__GPU_CAMERA_HPP
