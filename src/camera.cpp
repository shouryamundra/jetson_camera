// Purpose: Implement the Hikrobot camera over the MVS SDK.
// Paradigm: The SDK's grab thread buffers one frame; read() copies it into pinned memory and
// demosaics it on the GPU.
// Role: Supplies timestamped BGRA frames for GPU work, with no CPU pixel copy.

#include "camera.hpp"

#include <cstdio>
#include <stdexcept>

#include "MvCameraControl.h"
#include "demosaic.hpp"

namespace io
{
namespace
{
constexpr unsigned kTimeoutMs = 1000;

void require(const int status, const char * const what)
{
  if (status == MV_OK) return;
  char code[16];
  std::snprintf(code, sizeof code, "%#x", static_cast<unsigned>(status));
  throw std::runtime_error(std::string(what) + " failed: " + code);
}

Bayer bayer_of(const unsigned pixel_type)
{
  switch (pixel_type) {
    case PixelType_Gvsp_BayerRG8:
      return kRggb;
    case PixelType_Gvsp_BayerGR8:
      return kGrbg;
    case PixelType_Gvsp_BayerGB8:
      return kGbrg;
    case PixelType_Gvsp_BayerBG8:
      return kBggr;
    default:
      throw std::runtime_error("unsupported pixel format; set PixelFormat to an 8-bit Bayer type");
  }
}

}  // namespace

void Camera::Close::operator()(void * const handle) const noexcept
{
  MV_CC_StopGrabbing(handle);
  MV_CC_CloseDevice(handle);
  MV_CC_DestroyHandle(handle);
}

Camera::Camera(const std::string & ini_path)
{
  MV_CC_DEVICE_INFO_LIST devices{};
  require(MV_CC_EnumDevices(MV_USB_DEVICE, &devices), "MV_CC_EnumDevices");
  if (devices.nDeviceNum == 0) throw std::runtime_error("no USB camera found");

  void * handle = nullptr;
  require(MV_CC_CreateHandle(&handle, devices.pDeviceInfo[0]), "MV_CC_CreateHandle");
  handle_.reset(handle);  // closed on every exit path from here on
  require(MV_CC_OpenDevice(handle), "MV_CC_OpenDevice");
  require(MV_CC_FeatureLoad(handle, ini_path.c_str()), "MV_CC_FeatureLoad");

  const auto read_int = [handle](const char * const key) {
    MVCC_INTVALUE_EX value{};
    require(MV_CC_GetIntValueEx(handle, key, &value), key);
    return static_cast<int>(value.nCurValue);
  };
  MVCC_ENUMVALUE format{};
  require(MV_CC_GetEnumValue(handle, "PixelFormat", &format), "PixelFormat");
  width_ = read_int("Width");
  height_ = read_int("Height");
  payload_ = static_cast<unsigned>(read_int("PayloadSize"));
  bayer_ = bayer_of(format.nCurValue);

  raw_ = cuda::make_pinned<std::uint8_t>(payload_);
  bgra_ =
    cuda::make_pinned<uchar4>(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_));
  stream_ = cuda::make_stream();

  // A single SDK cache node, so frames do not queue up behind a slow reader. (The grab-strategy
  // API that would guarantee newest-only is Windows-only.)
  require(MV_CC_SetImageNodeNum(handle, 1), "MV_CC_SetImageNodeNum");
  require(MV_CC_StartGrabbing(handle), "MV_CC_StartGrabbing");
}

Camera::~Camera() { cudaStreamSynchronize(stream_.get()); }

std::optional<GpuFrame> Camera::read()
{
  // The previous frame's GPU work still reads raw_ and bgra_; only then may they be overwritten.
  cuda::check(cudaStreamSynchronize(stream_.get()), "cudaStreamSynchronize");

  MV_FRAME_OUT_INFO_EX info{};
  const int status =
    MV_CC_GetOneFrameTimeout(handle_.get(), raw_.get(), payload_, &info, kTimeoutMs);
  if (status != MV_OK || info.nFrameLen != payload_) return std::nullopt;
  const auto timestamp = std::chrono::steady_clock::now();

  launch_demosaic(raw_.get(), width_, height_, bayer_, bgra_.get(), stream_.get());
  return GpuFrame{bgra_.get(), width_, height_, stream_.get(), timestamp, info.nFrameNum};
}

}  // namespace io
