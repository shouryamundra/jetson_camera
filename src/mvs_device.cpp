// Purpose: Implement the MVS SDK wrapper for one Hikrobot USB camera.
// Paradigm: Object-oriented resource wrapper; setup throws, the streaming path does not.
// Role: Keeps every vendor call and error code out of the capture logic.

#include "mvs_device.hpp"

#include <cstdio>
#include <stdexcept>
#include <string>

#include "MvCameraControl.h"

namespace io
{
namespace
{
void require(const int status, const char * const what)
{
  if (status == MV_OK) return;
  char code[16];
  std::snprintf(code, sizeof code, "%#x", static_cast<unsigned>(status));
  throw std::runtime_error(std::string(what) + " failed: " + code);
}

std::uint32_t read_int(void * const handle, const char * const key)
{
  MVCC_INTVALUE_EX value;
  require(MV_CC_GetIntValueEx(handle, key, &value), key);
  return static_cast<std::uint32_t>(value.nCurValue);
}

std::uint32_t read_enum(void * const handle, const char * const key)
{
  MVCC_ENUMVALUE value;
  require(MV_CC_GetEnumValue(handle, key, &value), key);
  return value.nCurValue;
}

}  // namespace

void MvsDevice::HandleClose::operator()(void * const handle) const noexcept
{
  MV_CC_CloseDevice(handle);
  MV_CC_DestroyHandle(handle);
}

MvsDevice::MvsDevice(const std::string & ini_path)
{
  MV_CC_DEVICE_INFO_LIST devices{};
  require(MV_CC_EnumDevices(MV_USB_DEVICE, &devices), "MV_CC_EnumDevices");
  if (devices.nDeviceNum == 0) throw std::runtime_error("no USB camera found");

  void * handle = nullptr;
  require(MV_CC_CreateHandle(&handle, devices.pDeviceInfo[0]), "MV_CC_CreateHandle");
  handle_.reset(handle);  // from here on the handle is released even if a later call throws

  require(MV_CC_OpenDevice(handle), "MV_CC_OpenDevice");
  require(MV_CC_FeatureLoad(handle, ini_path.c_str()), "MV_CC_FeatureLoad");
  geometry_ = {
    .width = read_int(handle, "Width"),
    .height = read_int(handle, "Height"),
    .payload = read_int(handle, "PayloadSize"),
    .pixel_type = read_enum(handle, "PixelFormat")};
}

void MvsDevice::start_grabbing()
{
  require(MV_CC_StartGrabbing(handle_.get()), "MV_CC_StartGrabbing");
}

void MvsDevice::stop_grabbing() noexcept { MV_CC_StopGrabbing(handle_.get()); }

std::optional<std::uint32_t> MvsDevice::get_frame(
  const std::span<std::uint8_t> out, const std::chrono::milliseconds timeout) noexcept
{
  MV_FRAME_OUT_INFO_EX info{};
  const int status = MV_CC_GetOneFrameTimeout(
    handle_.get(), out.data(), static_cast<unsigned>(out.size()), &info,
    static_cast<unsigned>(timeout.count()));
  // A frame of a different size would be misread downstream.
  if (status != MV_OK || info.nFrameLen != geometry_.payload) return std::nullopt;
  return info.nFrameNum;
}

}  // namespace io
