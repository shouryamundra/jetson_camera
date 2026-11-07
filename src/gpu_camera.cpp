// Purpose: Capture raw Hikrobot frames into pinned memory and convert them on the GPU.
// Paradigm: RAII-owned pipeline with a capture thread and a lock-free frame handoff.
// Role: Supplies timestamped BGRA frames that GPU work can consume without any CPU copy.

#include "gpu_camera.hpp"

#include <cstdio>
#include <exception>
#include <stdexcept>

#include "PixelType.h"
#include "demosaic.hpp"

using namespace std::chrono_literals;

namespace io
{
namespace
{
constexpr auto kFrameTimeout = 100ms;  // a stalled camera is noticed within this
constexpr int kMaxFailures = 10;       // consecutive failed waits (about 1 s) before reopening
constexpr auto kRetryDelay = 100ms;

Bayer bayer_of(const std::uint32_t pixel_type)
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

GpuCamera::GpuCamera(const std::string & ini_path)
: ini_path_(ini_path),
  device_(std::make_unique<MvsDevice>(ini_path)),
  geometry_(device_->geometry()),
  bayer_(bayer_of(geometry_.pixel_type)),
  mailbox_([this] { return Slot{cuda::PinnedBuffer(geometry_.payload)}; }),
  output_(cuda::make_device_buffer<uchar4>(std::size_t{geometry_.width} * geometry_.height)),
  stream_(cuda::make_stream())
{
}

GpuCamera::~GpuCamera()
{
  stop();
  cudaStreamSynchronize(stream_.get());
}

void GpuCamera::start()
{
  if (thread_.joinable()) return;
  if (!device_) device_ = open();
  device_->start_grabbing();
  mailbox_.reset();
  thread_ = std::jthread([this](const std::stop_token stop) { capture(stop); });
}

void GpuCamera::stop()
{
  if (!thread_.joinable()) return;
  thread_.request_stop();
  thread_.join();
  mailbox_.close();
  if (device_) device_->stop_grabbing();
}

std::optional<GpuFrame> GpuCamera::read()
{
  // Everything queued on the stream for the previous frame, ours and the caller's, must be done
  // before acquire() hands that frame's raw slot back to the capture thread.
  cuda::check(cudaStreamSynchronize(stream_.get()), "cudaStreamSynchronize");
  const Slot * const slot = mailbox_.acquire();
  if (!slot) return std::nullopt;

  launch_demosaic(slot->raw.device(), width(), height(), bayer_, output_.get(), stream_.get());
  return GpuFrame{output_.get(), width(), height(), stream_.get(), slot->timestamp, slot->index};
}

std::unique_ptr<MvsDevice> GpuCamera::open() const
{
  auto device = std::make_unique<MvsDevice>(ini_path_);
  if (device->geometry() != geometry_) throw std::runtime_error("camera geometry changed");
  return device;
}

void GpuCamera::capture(const std::stop_token & stop)
{
  int failures = 0;
  while (!stop.stop_requested()) {
    Slot & slot = mailbox_.write_slot();
    const auto index = device_->get_frame(slot.raw.span(), kFrameTimeout);
    if (!index) {
      if (++failures >= kMaxFailures) {
        recover(stop);
        failures = 0;
      }
      continue;
    }
    failures = 0;
    slot.timestamp = std::chrono::steady_clock::now();
    slot.index = *index;
    mailbox_.publish();
  }
}

// Reopen after the camera stopped delivering frames, e.g. a replugged USB cable.
void GpuCamera::recover(const std::stop_token & stop)
{
  std::fputs("no frames, reopening the camera\n", stderr);
  device_.reset();  // the camera is opened exclusively, so release it before reopening
  while (!stop.stop_requested()) {
    std::this_thread::sleep_for(kRetryDelay);
    try {
      auto device = open();
      device->start_grabbing();
      device_ = std::move(device);
      return;
    } catch (const std::exception & e) {
      std::fprintf(stderr, "reopen failed: %s\n", e.what());
    }
  }
}

}  // namespace io
