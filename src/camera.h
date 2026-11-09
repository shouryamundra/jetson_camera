// A Hikrobot USB3 camera that publishes preprocessed, timestamped frames.
//
// Owns the SDK device, a grab thread, and a watchdog thread that owns the
// device lifecycle (open, detect loss or stall, close, reopen, USB reset).
// Consumers pull the newest frame from a mailbox; a callback sees every frame.

#ifndef JETSON_CAMERA_SRC_CAMERA_H_
#define JETSON_CAMERA_SRC_CAMERA_H_

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

#include "src/bayer.h"
#include "src/clock_sync.h"
#include "src/cuda_util.h"
#include "src/frame.h"

namespace jetson_camera {

// Defaults match config/pipeline.ini, which documents each setting.
struct CameraOptions {
  // [camera]
  std::string feature_file;  // MVS feature file applied after opening.
  double exposure_us = 0;    // 0 = keep the feature file's.
  double gain_db = -1;       // < 0 = keep the feature file's.
  int sensor_height = 812;   // Centred sensor rows; 0 = keep the file's.
  bool unlimited_frame_rate = true;
  int sdk_buffers = 8;
  int grab_timeout_ms = 100;
  // [output]
  int out_width = 960;  // Model input; the camera reads 1.5x this window.
  int out_height = 540;
  bool tensor = true;
  bool tensor_fp16 = true;
  bool tensor_rgb = false;   // false = BGR planes (Project Otto's order).
  bool shared_tile = false;  // Kernel load strategy; see preprocess.h.
  bool bgr = false;          // CPU-readable HxWx3; no CPU consumer by default.
  bool nv12 = true;          // Needed by the Recorder.
  int pool_size = 16;
  // [threads]
  bool realtime_priority = true;
  int realtime_level = 80;
  int cpu = -1;           // Pin the grab thread; -1 = no pinning.
  int watchdog_cpu = -1;  // Pin the watchdog thread; -1 = no pinning.
  bool lock_memory = true;
  // [timing]
  int latency_us = -1;  // -1 = rows * line_time_us + latency_base_us.
  double line_time_us = 5.653;
  int latency_base_us = 150;
  int clock_window_ms = 2000;
  int clock_reset_ms = 200;
  int clock_reset_frames = 5;
  // [watchdog]
  bool watchdog = true;
  int stall_ms = 500;
  int check_ms = 20;
  int retry_ms = 200;
  int retry_max_ms = 3000;
  int usb_reset_after = 3;  // Failed reopens before a USB reset; 0 = never.
};

struct CameraStats {
  std::uint64_t frames = 0;          // Published.
  std::uint64_t camera_gaps = 0;     // Lost between camera and us (estimated
                                     // across counter restarts).
  std::uint64_t counter_resets = 0;  // Camera frame counter restarted.
  std::uint64_t pool_drops = 0;      // Every slot was held by consumers.
  std::uint64_t errors = 0;          // SDK or CUDA errors on the hot path.
  std::uint64_t copy_fallbacks = 0;  // Extra copy because mapping failed.
  std::uint64_t disconnects = 0;     // Device-lost events from the SDK.
  std::uint64_t stalls = 0;          // No frame for stall_ms while connected.
  std::uint64_t reconnects = 0;      // Successful recoveries.
  std::uint64_t usb_resets = 0;      // USB port resets issued.
  bool connected = false;
};

class Camera {
 public:
  using FrameCallback = std::function<void(const FrameRef&)>;

  // Opens the first USB camera and starts capturing; throws
  // std::runtime_error if that fails. Afterwards the watchdog keeps it running
  // across disconnects and stalls. `on_frame` runs on the grab thread for
  // every frame: keep it short and non-blocking (Recorder::Push qualifies).
  explicit Camera(const CameraOptions& options, FrameCallback on_frame = {});
  // Consumers holding frames (e.g. a Recorder fed by on_frame) must be
  // destroyed before the Camera; call Stop() first.
  ~Camera();
  Camera(const Camera&) = delete;
  Camera& operator=(const Camera&) = delete;

  // Stops capture and the watchdog and wakes waiting consumers. Idempotent.
  void Stop();

  // The newest frame with seq > after_seq, waiting up to `timeout`.
  [[nodiscard]] std::optional<FrameRef> Latest(
      std::uint64_t after_seq, std::chrono::microseconds timeout) {
    return mailbox_.WaitNewer(after_seq, timeout);
  }

  [[nodiscard]] CameraStats stats() const;
  [[nodiscard]] int width() const { return options_.out_width; }
  [[nodiscard]] int height() const { return options_.out_height; }
  [[nodiscard]] double sensor_fps() const { return sensor_fps_; }
  [[nodiscard]] double exposure_us() const { return exposure_us_; }
  [[nodiscard]] double gain_db() const { return gain_db_; }
  [[nodiscard]] int latency_us() const { return latency_us_; }
  [[nodiscard]] bool realtime() const { return realtime_; }
  [[nodiscard]] bool pinned() const { return pinned_; }
  [[nodiscard]] bool memory_locked() const { return memory_locked_; }
  [[nodiscard]] const CameraOptions& options() const { return options_; }

 private:
  // Lifecycle: only called while the grab thread is not running.
  void OpenDevice();
  void CloseDevice() noexcept;
  void StartGrabbing();
  void StopGrabbing() noexcept;
  void Recover();
  bool UsbReset() noexcept;

  void GrabLoop() noexcept;
  void WatchdogLoop() noexcept;
  // Processes one SDK frame end to end; never throws.
  void ProcessFrame(void* sdk_frame) noexcept;
  const std::uint8_t* GpuView(const std::uint8_t* host,
                              std::size_t bytes) noexcept;
  static void OnException(unsigned int type, void* self);
  // Sleeps for `d` unless Stop() is called; returns false if stopped.
  [[nodiscard]] bool SleepUnlessStopped(std::chrono::milliseconds d);

  CameraOptions options_;
  FrameCallback on_frame_;
  void* handle_ = nullptr;  // MVS device handle.
  int raw_width_ = 0;
  int raw_height_ = 0;
  Bayer bayer_{};
  double sensor_fps_ = 0;
  double exposure_us_ = 0;
  double gain_db_ = 0;
  int latency_us_ = 0;

  FramePool pool_;
  LatestFrame mailbox_;
  ClockSync clock_;
  cuda::Stream stream_;
  cuda::Event start_;
  cuda::Event stop_;
  // SDK receive buffer -> its GPU pointer (each buffer mapped once).
  std::unordered_map<const void*, const std::uint8_t*> mapped_;
  cuda::Device<std::uint8_t> staging_;  // Only if mapping fails.
  std::size_t staging_bytes_ = 0;

  // Grab-thread state (persists across reconnects).
  std::uint64_t seq_ = 0;
  std::optional<std::uint32_t> last_camera_frame_;

  std::atomic<std::uint64_t> frames_{0};
  std::atomic<std::uint64_t> gaps_{0};
  std::atomic<std::uint64_t> counter_resets_{0};
  std::atomic<std::uint64_t> pool_drops_{0};
  std::atomic<std::uint64_t> errors_{0};
  std::atomic<std::uint64_t> fallbacks_{0};
  std::atomic<std::uint64_t> disconnects_{0};
  std::atomic<std::uint64_t> stalls_{0};
  std::atomic<std::uint64_t> reconnects_{0};
  std::atomic<std::uint64_t> usb_resets_{0};
  std::atomic<std::int64_t> last_frame_us_{0};  // Steady clock.
  std::atomic<bool> connected_{false};
  std::atomic<bool> device_lost_{false};
  std::atomic<bool> realtime_{false};
  std::atomic<bool> pinned_{false};
  std::atomic<bool> memory_locked_{false};
  std::atomic<bool> grab_stop_{false};
  std::atomic<bool> stop_requested_{false};
  std::mutex stop_mutex_;
  std::condition_variable stop_cv_;
  std::thread grab_thread_;
  std::thread watchdog_thread_;
};

}  // namespace jetson_camera

#endif  // JETSON_CAMERA_SRC_CAMERA_H_
