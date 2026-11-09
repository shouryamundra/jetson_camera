// Hikrobot camera over MVS SDK 4.9, with a self-healing watchdog.
//
// Grab thread: newest SDK buffer (LatestImagesOnly) -> fused kernel straight
// on it (the buffer is mapped into the GPU once) -> hand back -> publish.
// Watchdog thread: owns open/close; on an SDK disconnect or a stall it stops
// the grab thread, closes, and reopens with backoff, escalating to a USB
// port reset. Exceptions are used only for setup; the grab path never throws.

#include "src/camera.h"

#include <dirent.h>
#include <fcntl.h>
#include <linux/usbdevice_fs.h>
#include <pthread.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>

#include "MvCameraControl.h"
#include "src/preprocess.h"

namespace jetson_camera {
namespace {

constexpr char kUsbVendorHikrobot[] = "2bdf";
constexpr std::int64_t kFirstFrameGraceUs = 2'000'000;

void Require(int status, const char* what) {
  if (status == MV_OK) return;
  char code[16];
  std::snprintf(code, sizeof(code), "%#x", static_cast<unsigned>(status));
  throw std::runtime_error(std::string(what) + " failed: " + code);
}

Bayer BayerOf(unsigned pixel_type) {
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
      throw std::runtime_error(
          "unsupported pixel format; set PixelFormat to an 8-bit Bayer type");
  }
}

std::int64_t RealtimeUs() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

std::int64_t SteadyUs() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

// MVS needs one process-wide Initialize/Finalize pair.
void EnsureSdk() {
  struct SdkLifetime {
    SdkLifetime() { Require(MV_CC_Initialize(), "MV_CC_Initialize"); }
    ~SdkLifetime() { MV_CC_Finalize(); }
  };
  static SdkLifetime sdk;
}

FramePool::Layout LayoutOf(const CameraOptions& o) {
  return {o.out_width, o.out_height, o.tensor, o.tensor_fp16, o.bgr, o.nv12};
}

bool PinTo(int cpu) {
  if (cpu < 0) return false;
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(static_cast<std::size_t>(cpu), &set);
  return pthread_setaffinity_np(pthread_self(), sizeof(set), &set) == 0;
}

std::string ReadLine(const std::string& path) {
  std::ifstream f(path);
  std::string s;
  std::getline(f, s);
  return s;
}

}  // namespace

Camera::Camera(const CameraOptions& options, FrameCallback on_frame)
    : options_(options),
      on_frame_(std::move(on_frame)),
      pool_(static_cast<std::size_t>(options.pool_size), LayoutOf(options)),
      clock_(std::int64_t{options.clock_window_ms} * 1000,
             std::int64_t{options.clock_reset_ms} * 1000,
             options.clock_reset_frames),
      // Most urgent priority: preprocessing never queues behind inference.
      stream_(cuda::MakeStream(cuda::HighestStreamPriority())),
      start_(cuda::MakeEvent()),
      stop_(cuda::MakeEvent()) {
  if (options.out_width % 2 != 0 || options.out_height % 2 != 0) {
    throw std::invalid_argument("output size must be even");
  }
  EnsureSdk();
  if (options.lock_memory) {
    // MCL_ONFAULT locks pages as they are touched instead of populating
    // CUDA's large virtual reservations up front.
    memory_locked_ = mlockall(MCL_CURRENT | MCL_FUTURE | MCL_ONFAULT) == 0;
  }
  OpenDevice();
  StartGrabbing();
  if (options.watchdog) {
    watchdog_thread_ = std::thread([this] { WatchdogLoop(); });
  }
}

void Camera::Stop() {
  {
    std::lock_guard lock(stop_mutex_);
    stop_requested_ = true;
  }
  stop_cv_.notify_all();
  if (watchdog_thread_.joinable())
    watchdog_thread_.join();  // May be mid-recovery.
  StopGrabbing();
  mailbox_.Close();
}

Camera::~Camera() {
  Stop();
  CloseDevice();
}

CameraStats Camera::stats() const {
  CameraStats s;
  s.frames = frames_;
  s.camera_gaps = gaps_;
  s.counter_resets = counter_resets_;
  s.pool_drops = pool_drops_;
  s.errors = errors_;
  s.copy_fallbacks = fallbacks_;
  s.disconnects = disconnects_;
  s.stalls = stalls_;
  s.reconnects = reconnects_;
  s.usb_resets = usb_resets_;
  s.connected = connected_;
  return s;
}

bool Camera::SleepUnlessStopped(std::chrono::milliseconds d) {
  std::unique_lock lock(stop_mutex_);
  return !stop_cv_.wait_for(lock, d, [this] { return stop_requested_.load(); });
}

void Camera::OnException(unsigned int type, void* self) {
  // Runs on an SDK thread: only flag it; the watchdog does the work.
  auto* camera = static_cast<Camera*>(self);
  if (type == MV_EXCEPTION_DEV_DISCONNECT) ++camera->disconnects_;
  camera->device_lost_ = true;
}

void Camera::OpenDevice() {
  MV_CC_DEVICE_INFO_LIST devices{};
  Require(MV_CC_EnumDevices(MV_USB_DEVICE, &devices), "MV_CC_EnumDevices");
  if (devices.nDeviceNum == 0) throw std::runtime_error("no USB camera found");

  void* handle = nullptr;
  Require(MV_CC_CreateHandle(&handle, devices.pDeviceInfo[0]),
          "MV_CC_CreateHandle");
  handle_ = handle;
  try {
    const CameraOptions& o = options_;
    Require(MV_CC_OpenDevice(handle), "MV_CC_OpenDevice");
    if (!o.feature_file.empty()) {
      Require(MV_CC_FeatureLoad(handle, o.feature_file.c_str()),
              "MV_CC_FeatureLoad");
    }
    if (o.exposure_us > 0) {
      MV_CC_SetEnumValue(handle, "ExposureAuto", 0);
      Require(MV_CC_SetFloatValue(handle, "ExposureTime",
                                  static_cast<float>(o.exposure_us)),
              "ExposureTime");
    }
    if (o.gain_db >= 0) {
      MV_CC_SetEnumValue(handle, "GainAuto", 0);
      Require(
          MV_CC_SetFloatValue(handle, "Gain", static_cast<float>(o.gain_db)),
          "Gain");
    }
    if (o.sensor_height > 0) {
      MVCC_INTVALUE_EX height_max{};
      Require(MV_CC_GetIntValueEx(handle, "HeightMax", &height_max),
              "HeightMax");
      Require(MV_CC_SetIntValueEx(handle, "OffsetY", 0), "OffsetY");
      Require(MV_CC_SetIntValueEx(handle, "Height", o.sensor_height), "Height");
      // Offset a multiple of 4 (the sensor step), so the Bayer phase holds.
      Require(MV_CC_SetIntValueEx(
                  handle, "OffsetY",
                  (height_max.nCurValue - o.sensor_height) / 2 / 4 * 4),
              "OffsetY");
    }
    if (o.unlimited_frame_rate) {
      MV_CC_SetBoolValue(handle, "AcquisitionFrameRateEnable", false);
    }

    const auto read_int = [handle](const char* key) {
      MVCC_INTVALUE_EX value{};
      Require(MV_CC_GetIntValueEx(handle, key, &value), key);
      return static_cast<int>(value.nCurValue);
    };
    const auto read_float = [handle](const char* key) {
      MVCC_FLOATVALUE value{};
      Require(MV_CC_GetFloatValue(handle, key, &value), key);
      return static_cast<double>(value.fCurValue);
    };
    MVCC_ENUMVALUE format{};
    Require(MV_CC_GetEnumValue(handle, "PixelFormat", &format), "PixelFormat");
    const Bayer bayer = BayerOf(format.nCurValue);
    const int width = read_int("Width");
    const int height = read_int("Height");
    if (raw_width_ != 0 && (width != raw_width_ || height != raw_height_)) {
      throw std::runtime_error("camera geometry changed on reconnect");
    }
    if (width < o.out_width * 3 / 2 || height < o.out_height * 3 / 2) {
      throw std::runtime_error("sensor window smaller than 1.5x the output");
    }
    bayer_ = bayer;
    raw_width_ = width;
    raw_height_ = height;
    sensor_fps_ = read_float("ResultingFrameRate");
    exposure_us_ = read_float("ExposureTime");
    gain_db_ = read_float("Gain");
    latency_us_ = o.latency_us >= 0
                      ? o.latency_us
                      : static_cast<int>(raw_height_ * o.line_time_us) +
                            o.latency_base_us;

    Require(MV_CC_RegisterExceptionCallBack(handle, &Camera::OnException, this),
            "MV_CC_RegisterExceptionCallBack");
    Require(MV_CC_SetImageNodeNum(handle, static_cast<unsigned>(o.sdk_buffers)),
            "MV_CC_SetImageNodeNum");
    Require(MV_CC_SetGrabStrategy(handle, MV_GrabStrategy_LatestImagesOnly),
            "MV_CC_SetGrabStrategy");
    Require(MV_CC_StartGrabbing(handle), "MV_CC_StartGrabbing");
  } catch (...) {
    CloseDevice();
    throw;
  }
  device_lost_ = false;
  connected_ = true;
}

void Camera::CloseDevice() noexcept {
  if (handle_ == nullptr) return;
  for (const auto& [host, gpu] : mapped_) {
    cudaHostUnregister(const_cast<void*>(host));
  }
  mapped_.clear();  // A reopened device allocates new receive buffers.
  MV_CC_StopGrabbing(handle_);
  MV_CC_CloseDevice(handle_);
  MV_CC_DestroyHandle(handle_);
  handle_ = nullptr;
  connected_ = false;
}

void Camera::StartGrabbing() {
  grab_stop_ = false;
  last_camera_frame_.reset();
  // The first frame after (re)opening may take a while: allow
  // kFirstFrameGraceUs before calling it a stall.
  last_frame_us_ =
      SteadyUs() +
      std::max<std::int64_t>(
          0, kFirstFrameGraceUs - std::int64_t{options_.stall_ms} * 1000);
  grab_thread_ = std::thread([this] { GrabLoop(); });
}

void Camera::StopGrabbing() noexcept {
  grab_stop_ = true;
  if (grab_thread_.joinable()) grab_thread_.join();
}

// GPU pointer for an SDK receive buffer. The SDK reuses a fixed set of
// buffers, so each is page-locked and mapped once (~1-2 ms) and then costs
// nothing. Falls back to a copy if mapping fails; nullptr if that fails too.
const std::uint8_t* Camera::GpuView(const std::uint8_t* host,
                                    std::size_t bytes) noexcept {
  if (const auto it = mapped_.find(host); it != mapped_.end())
    return it->second;
  auto* mutable_host = const_cast<std::uint8_t*>(host);
  void* device = nullptr;
  // Read-only mapping first (lets the GPU cache it); plain mapped otherwise.
  constexpr unsigned kRegisterFlags[] = {
      cudaHostRegisterMapped | cudaHostRegisterReadOnly,
      cudaHostRegisterMapped};
  for (const unsigned flags : kRegisterFlags) {
    if (cudaHostRegister(mutable_host, bytes, flags) == cudaSuccess) {
      if (cudaHostGetDevicePointer(&device, mutable_host, 0) == cudaSuccess) {
        try {
          return mapped_[host] = static_cast<const std::uint8_t*>(device);
        } catch (...) {  // Map insertion failed (out of memory).
        }
      }
      cudaHostUnregister(mutable_host);
    }
    cudaGetLastError();  // Clear the error from the failed attempt.
  }
  if (staging_bytes_ < bytes) {
    void* p = nullptr;
    if (cudaMalloc(&p, bytes) != cudaSuccess) return nullptr;
    staging_.reset(static_cast<std::uint8_t*>(p));
    staging_bytes_ = bytes;
  }
  if (cudaMemcpyAsync(staging_.get(), host, bytes, cudaMemcpyHostToDevice,
                      stream_.get()) != cudaSuccess) {
    return nullptr;
  }
  ++fallbacks_;
  return staging_.get();
}

void Camera::GrabLoop() noexcept {
  if (options_.realtime_priority) {
    sched_param param{};
    param.sched_priority = options_.realtime_level;
    realtime_ = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) == 0;
  }
  pinned_ = PinTo(options_.cpu);
  pthread_setname_np(pthread_self(), "camera-grab");

  const auto timeout = static_cast<unsigned>(options_.grab_timeout_ms);
  while (!grab_stop_.load(std::memory_order_relaxed)) {
    MV_FRAME_OUT frame{};
    const auto status =
        static_cast<unsigned>(MV_CC_GetImageBuffer(handle_, &frame, timeout));
    if (status != MV_OK) {
      if (status != MV_E_NODATA) ++errors_;
      if (status == MV_E_DEV_DISCONNECT) device_lost_ = true;
      continue;
    }
    ProcessFrame(&frame);
    MV_CC_FreeImageBuffer(handle_, &frame);  // Always returned to the SDK.
  }
}

void Camera::ProcessFrame(void* sdk_frame) noexcept {
  const auto& frame = *static_cast<const MV_FRAME_OUT*>(sdk_frame);
  const MV_FRAME_OUT_INFO_EX& fi = frame.stFrameInfo;
  const std::int64_t arrival = RealtimeUs();
  const std::int64_t now = SteadyUs();
  const std::int64_t previous =
      last_frame_us_.exchange(now, std::memory_order_relaxed);

  if (last_camera_frame_ && fi.nFrameNum > *last_camera_frame_ + 1) {
    gaps_ += fi.nFrameNum - *last_camera_frame_ - 1;
  } else if (last_camera_frame_ && fi.nFrameNum <= *last_camera_frame_) {
    // Counter restarted (USB reset, camera reboot): estimate loss from time.
    ++counter_resets_;
    const double period_us = 1e6 / std::max(sensor_fps_, 1.0);
    gaps_ += static_cast<std::uint64_t>(
        std::max(0.0, static_cast<double>(now - previous) / period_us - 0.5));
    clock_.Reset();  // The camera clock restarted too.
  }
  last_camera_frame_ = fi.nFrameNum;

  const auto expected = static_cast<std::uint64_t>(raw_width_) *
                        static_cast<std::uint64_t>(raw_height_);
  if (fi.nFrameLenEx < expected) {
    ++errors_;
    return;
  }
  std::optional<FrameRef> slot = pool_.Acquire();
  if (!slot) {
    ++pool_drops_;
    return;
  }
  const std::uint8_t* raw =
      GpuView(frame.pBufAddr, static_cast<std::size_t>(fi.nFrameLenEx));
  if (raw == nullptr) {
    ++errors_;
    return;
  }

  const int src_w = options_.out_width * 3 / 2;
  const int src_h = options_.out_height * 3 / 2;
  const PreprocessParams params{raw,
                                raw_width_,
                                raw_height_,
                                bayer_,
                                (raw_width_ - src_w) / 2,
                                (raw_height_ - src_h) / 2,
                                options_.out_width,
                                options_.out_height,
                                options_.tensor_fp16,
                                options_.tensor_rgb,
                                options_.shared_tile};
  cudaStream_t stream = stream_.get();
  // The caller frees the SDK buffer right after this returns, so the kernel
  // must be finished: wait on the event (~0.2 ms; the frame period is >= 4.6).
  if (cudaEventRecord(start_.get(), stream) != cudaSuccess ||
      LaunchPreprocess(params, slot->buffers(), stream) != cudaSuccess ||
      cudaEventRecord(stop_.get(), stream) != cudaSuccess ||
      cudaEventSynchronize(stop_.get()) != cudaSuccess) {
    ++errors_;
    cudaGetLastError();
    return;
  }

  FrameInfo& info = pool_.MutableInfo(*slot);
  const std::uint64_t ticks =
      (static_cast<std::uint64_t>(fi.nDevTimeStampHigh) << 32) |
      fi.nDevTimeStampLow;
  const auto exposure = static_cast<std::int64_t>(
      fi.fExposureTime > 0 ? fi.fExposureTime : exposure_us_);
  // Stamp = exposure start, 10 ns ticks; arrival = stamp + exposure +
  // readout/USB latency + jitter.
  info.exposure_start_us = clock_.ToHost(static_cast<std::int64_t>(ticks / 100),
                                         arrival - exposure, latency_us_);
  info.exposure_us = exposure;
  info.seq = ++seq_;
  info.camera_frame = fi.nFrameNum;
  info.arrival_us = arrival;
  if (cudaEventElapsedTime(&info.gpu_ms, start_.get(), stop_.get()) !=
      cudaSuccess) {
    info.gpu_ms = -1;
  }
  info.ready_us = RealtimeUs();

  ++frames_;
  if (on_frame_) {
    try {
      on_frame_(*slot);
    } catch (...) {  // A consumer bug must not stop capture.
      ++errors_;
    }
  }
  mailbox_.Publish(std::move(*slot));
}

void Camera::WatchdogLoop() noexcept {
  PinTo(options_.watchdog_cpu);
  pthread_setname_np(pthread_self(), "camera-watchdog");
  const auto period = std::chrono::milliseconds(options_.check_ms);
  while (SleepUnlessStopped(period)) {
    const bool lost = device_lost_;
    const bool stalled =
        SteadyUs() - last_frame_us_ > std::int64_t{options_.stall_ms} * 1000;
    if (!lost && !stalled) continue;
    if (stalled && !lost) ++stalls_;
    std::fprintf(stderr, "camera watchdog: %s, recovering\n",
                 lost ? "device lost" : "no frames (stall)");
    try {
      Recover();
    } catch (const std::exception& e) {  // Only on resource exhaustion.
      std::fprintf(stderr, "camera watchdog: recovery error: %s\n", e.what());
    }
  }
}

// Stops the grab thread, closes the device, then reopens with exponential
// backoff; every usb_reset_after failed attempts it resets the camera's USB
// port. Runs until success or Stop().
void Camera::Recover() {
  StopGrabbing();
  CloseDevice();
  clock_.Reset();  // The camera clock restarts on reconnect.
  auto delay = std::chrono::milliseconds(options_.retry_ms);
  for (int attempt = 1; !stop_requested_; ++attempt) {
    try {
      OpenDevice();
      StartGrabbing();
      ++reconnects_;
      std::fprintf(stderr, "camera watchdog: reconnected after %d attempt(s)\n",
                   attempt);
      return;
    } catch (const std::exception& e) {
      std::fprintf(stderr, "camera watchdog: attempt %d failed: %s\n", attempt,
                   e.what());
    }
    if (options_.usb_reset_after > 0 &&
        attempt % options_.usb_reset_after == 0 && UsbReset()) {
      ++usb_resets_;
    }
    if (!SleepUnlessStopped(delay)) return;
    delay =
        std::min(delay * 2, std::chrono::milliseconds(options_.retry_max_ms));
  }
}

// Software USB port reset (USBDEVFS_RESET) of the first Hikrobot device, like
// a replug for most faults. Needs write access to /dev/bus/usb/BBB/DDD (the
// MVS udev rule grants it).
bool Camera::UsbReset() noexcept {
  DIR* dir = opendir("/sys/bus/usb/devices");
  if (dir == nullptr) return false;
  bool done = false;
  while (const dirent* e = readdir(dir)) {
    try {
      const std::string base = std::string("/sys/bus/usb/devices/") + e->d_name;
      if (ReadLine(base + "/idVendor") != kUsbVendorHikrobot) continue;
      char node[64];
      std::snprintf(node, sizeof(node), "/dev/bus/usb/%03d/%03d",
                    std::atoi(ReadLine(base + "/busnum").c_str()),
                    std::atoi(ReadLine(base + "/devnum").c_str()));
      const int fd = open(node, O_WRONLY);
      if (fd < 0) continue;
      done = ioctl(fd, USBDEVFS_RESET, 0) == 0;
      close(fd);
      std::fprintf(stderr, "camera watchdog: USB reset of %s %s\n", node,
                   done ? "done" : "failed");
    } catch (...) {
      continue;
    }
    break;
  }
  closedir(dir);
  return done;
}

}  // namespace jetson_camera
