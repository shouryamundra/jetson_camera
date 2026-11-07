// Purpose: Capture raw Hikrobot frames into pinned memory and convert them on the GPU.
// Paradigm: Object-oriented SDK adapter with a capture thread and a lock-free frame handoff.
// Role: Supplies timestamped BGRA frames that GPU work can consume without any CPU copy.

#include "gpu_camera.hpp"

#include <cstdio>
#include <stdexcept>
#include <string>

#include "MvCameraControl.h"

using namespace std::chrono_literals;

namespace io
{
namespace
{
constexpr unsigned kTimeoutMs = 100;  // per-frame wait; a stalled camera is noticed within this
constexpr int kMaxFailures = 10;      // consecutive failed waits (~1 s) before reopening the device

bool ok(int ret, const char * what)
{
  if (ret == MV_OK) return true;
  std::fprintf(stderr, "%s failed: %#x\n", what, static_cast<unsigned>(ret));
  return false;
}

void check(cudaError_t error, const char * what)
{
  if (error != cudaSuccess)
    throw std::runtime_error(std::string(what) + ": " + cudaGetErrorString(error));
}

bool read_int(void * handle, const char * key, unsigned & value)
{
  MVCC_INTVALUE_EX v;
  if (!ok(MV_CC_GetIntValueEx(handle, key, &v), key)) return false;
  value = static_cast<unsigned>(v.nCurValue);
  return true;
}

bool bayer_of(unsigned pixel_type, Bayer & pattern)
{
  switch (pixel_type) {
    case PixelType_Gvsp_BayerRG8: pattern = {0, 0}; return true;
    case PixelType_Gvsp_BayerGR8: pattern = {0, 1}; return true;
    case PixelType_Gvsp_BayerGB8: pattern = {1, 0}; return true;
    case PixelType_Gvsp_BayerBG8: pattern = {1, 1}; return true;
    default: return false;
  }
}

}  // namespace

GpuCamera::GpuCamera(const std::string & ini_path, Roi roi) : ini_path_(ini_path), roi_(roi)
{
  // Spin while waiting on the GPU for the lowest wake-up latency. This only takes effect before
  // the CUDA context exists; if the host application already created one, keep its settings.
  if (cudaSetDeviceFlags(cudaDeviceScheduleSpin | cudaDeviceMapHost) == cudaErrorSetOnActiveProcess)
    cudaGetLastError();

  if (!open_device(geometry_)) throw std::runtime_error("cannot open camera");
  if (!bayer_of(geometry_.pixel_type, bayer_))
    throw std::runtime_error("unsupported pixel format; set PixelFormat to an 8-bit Bayer type");

  if (roi_.width == 0 || roi_.height == 0)
    roi_ = {0, 0, int(geometry_.width), int(geometry_.height)};
  if (
    roi_.x < 0 || roi_.y < 0 || roi_.width < 0 || roi_.height < 0 ||
    roi_.x + roi_.width > int(geometry_.width) || roi_.y + roi_.height > int(geometry_.height))
    throw std::invalid_argument("roi does not fit the camera frame");

  check(cudaStreamCreateWithFlags(&stream_, cudaStreamNonBlocking), "cudaStreamCreate");
  check(cudaMalloc(&output_, size_t(roi_.width) * roi_.height * sizeof(uchar4)), "cudaMalloc");
  for (Slot & slot : mailbox_.slots()) {
    void * host = nullptr;
    void * device = nullptr;
    check(cudaHostAlloc(&host, geometry_.payload, cudaHostAllocMapped), "cudaHostAlloc");
    check(cudaHostGetDevicePointer(&device, host, 0), "cudaHostGetDevicePointer");
    slot.host = static_cast<uint8_t *>(host);
    slot.device = static_cast<const uint8_t *>(device);
  }
}

GpuCamera::~GpuCamera()
{
  stop();
  close_device();
  for (Slot & slot : mailbox_.slots()) cudaFreeHost(slot.host);
  cudaFree(output_);
  if (stream_) cudaStreamDestroy(stream_);
}

void GpuCamera::start()
{
  if (capture_thread_.joinable()) return;
  if (!ok(MV_CC_StartGrabbing(handle_), "MV_CC_StartGrabbing"))
    throw std::runtime_error("cannot start acquisition");
  mailbox_.reset();
  quit_ = false;
  capture_thread_ = std::thread([this] { capture_loop(); });
}

void GpuCamera::stop()
{
  if (!capture_thread_.joinable()) return;
  quit_ = true;
  capture_thread_.join();
  if (handle_) MV_CC_StopGrabbing(handle_);
  mailbox_.close();
}

bool GpuCamera::read(GpuFrame & frame)
{
  // Everything queued on the stream for the previous frame, ours and the caller's, must be done
  // before acquire() hands that frame's raw slot back to the capture thread.
  check(cudaStreamSynchronize(stream_), "cudaStreamSynchronize");
  const Slot * slot = mailbox_.acquire();
  if (!slot) return false;

  check(
    launch_demosaic(
      slot->device, int(geometry_.width), int(geometry_.height), bayer_, roi_, output_, stream_),
    "demosaic launch");
  frame = {output_,        roi_.width, roi_.height, size_t(roi_.width) * sizeof(uchar4),
           stream_,        slot->timestamp,         slot->index};
  return true;
}

bool GpuCamera::open_device(Geometry & geometry)
{
  MV_CC_DEVICE_INFO_LIST devices{};
  if (!ok(MV_CC_EnumDevices(MV_USB_DEVICE, &devices), "MV_CC_EnumDevices")) return false;
  if (devices.nDeviceNum == 0) {
    std::fprintf(stderr, "no USB camera found\n");
    return false;
  }

  if (
    !ok(MV_CC_CreateHandle(&handle_, devices.pDeviceInfo[0]), "MV_CC_CreateHandle") ||
    !ok(MV_CC_OpenDevice(handle_), "MV_CC_OpenDevice") ||
    !ok(MV_CC_FeatureLoad(handle_, ini_path_.c_str()), "MV_CC_FeatureLoad") ||
    !read_int(handle_, "Width", geometry.width) || !read_int(handle_, "Height", geometry.height) ||
    !read_int(handle_, "PayloadSize", geometry.payload)) {
    close_device();
    return false;
  }

  MVCC_ENUMVALUE format;
  if (!ok(MV_CC_GetEnumValue(handle_, "PixelFormat", &format), "PixelFormat")) {
    close_device();
    return false;
  }
  geometry.pixel_type = format.nCurValue;
  return true;
}

void GpuCamera::close_device()
{
  if (!handle_) return;
  MV_CC_CloseDevice(handle_);
  MV_CC_DestroyHandle(handle_);
  handle_ = nullptr;
}

void GpuCamera::capture_loop()
{
  int failures = 0;
  while (!quit_) {
    Slot & slot = mailbox_.write_slot();
    MV_FRAME_OUT_INFO_EX info{};
    const int ret =
      MV_CC_GetOneFrameTimeout(handle_, slot.host, geometry_.payload, &info, kTimeoutMs);
    if (ret != MV_OK) {
      if (++failures >= kMaxFailures) {
        recover();
        failures = 0;
      }
      continue;
    }
    failures = 0;

    // A frame of a different size would be misread by the kernel.
    if (info.nFrameLen != geometry_.payload) continue;

    slot.timestamp = std::chrono::steady_clock::now();
    slot.index = info.nFrameNum;
    mailbox_.publish();
  }
}

// Reopen after the camera stopped delivering frames, e.g. a replugged USB cable.
void GpuCamera::recover()
{
  std::fprintf(stderr, "no frames, reopening the camera\n");
  if (handle_) MV_CC_StopGrabbing(handle_);
  close_device();
  while (!quit_) {
    std::this_thread::sleep_for(100ms);
    Geometry geometry;
    if (
      open_device(geometry) && geometry == geometry_ &&
      ok(MV_CC_StartGrabbing(handle_), "MV_CC_StartGrabbing"))
      return;
    close_device();
  }
}

}  // namespace io
