// Preallocated frame slots shared by the camera, the detector and the recorder.
//
// A fixed pool with intrusive atomic reference counts (an allocation-free
// shared_ptr) and a single-slot "latest value" mailbox: every consumer holds a
// finished frame as long as it needs, while the camera always publishes the
// newest one and never waits for a consumer.

#ifndef JETSON_CAMERA_SRC_FRAME_H_
#define JETSON_CAMERA_SRC_FRAME_H_

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace jetson_camera {

// Timing and identity of one frame. All *_us times are CLOCK_REALTIME
// microseconds, the clock Project Otto's JetsonTimestamp uses.
struct FrameInfo {
  std::uint64_t seq = 0;               // Publish order; a gap = dropped by us.
  std::uint32_t camera_frame = 0;      // Camera counter; a gap = lost on USB.
  std::int64_t exposure_start_us = 0;  // From the camera clock (ClockSync).
  std::int64_t exposure_us = 0;
  std::int64_t arrival_us = 0;  // The SDK handed the frame to the grab thread.
  std::int64_t ready_us = 0;    // All outputs written, visible to CPU and GPU.
  float gpu_ms = 0;             // Preprocess kernel time.

  [[nodiscard]] std::int64_t ExposureMidUs() const {
    return exposure_start_us + exposure_us / 2;
  }
};

// Where one slot's outputs live. Any pointer is null when that output is off.
struct FrameBuffers {
  void* tensor = nullptr;        // Device: 1x3xHxW planar fp16/fp32 in [0,1].
  std::uint8_t* bgr = nullptr;   // Pinned: HxWx3 BGR, CPU-readable, no copy.
  std::uint8_t* nv12 = nullptr;  // Pinned: W*H luma + W*H/2 chroma, BT.709.
};

class FramePool;

// Shared, read-only handle to a published frame. Copying adds a reference;
// the slot returns to the pool when the last handle is destroyed (any thread).
class FrameRef {
 public:
  FrameRef(const FrameRef& other) noexcept;
  FrameRef(FrameRef&& other) noexcept
      : pool_(std::exchange(other.pool_, nullptr)), index_(other.index_) {}
  FrameRef& operator=(FrameRef other) noexcept {
    std::swap(pool_, other.pool_);
    std::swap(index_, other.index_);
    return *this;
  }
  ~FrameRef();

  [[nodiscard]] const FrameInfo& info() const;
  [[nodiscard]] const FrameBuffers& buffers() const;

 private:
  friend class FramePool;
  FrameRef(FramePool* pool, std::uint32_t index) noexcept
      : pool_(pool), index_(index) {}

  FramePool* pool_;
  std::uint32_t index_;
};

class FramePool {
 public:
  struct Layout {
    int width = 0;
    int height = 0;
    bool tensor = true;
    bool tensor_fp16 = true;
    bool bgr = true;
    bool nv12 = true;
  };

  // Allocates every slot up front (throws std::runtime_error on CUDA failure).
  FramePool(std::size_t count, const Layout& layout);
  ~FramePool();
  FramePool(const FramePool&) = delete;
  FramePool& operator=(const FramePool&) = delete;

  // A free slot for the producer to fill, or nothing if all are referenced.
  [[nodiscard]] std::optional<FrameRef> Acquire() noexcept;
  // Producer-only: the slot's info while it is filled, before it is shared.
  [[nodiscard]] FrameInfo& MutableInfo(const FrameRef& frame) noexcept {
    return slots_[frame.index_].info;
  }
  [[nodiscard]] const Layout& layout() const { return layout_; }
  [[nodiscard]] std::size_t free_count() const;

 private:
  friend class FrameRef;
  struct Slot {
    std::atomic<int> refs{0};
    FrameInfo info;
    FrameBuffers buffers;
  };
  void Release(std::uint32_t index) noexcept;

  Layout layout_;
  std::size_t count_;
  std::unique_ptr<Slot[]> slots_;
  void* device_block_ = nullptr;
  void* pinned_block_ = nullptr;
  mutable std::mutex free_mutex_;    // Guards free_; a few ops per frame.
  std::vector<std::uint32_t> free_;  // LIFO: the last freed slot is cache-warm.
};

// Single-slot mailbox holding the newest frame. Publishing replaces (and
// releases) the previous frame, so a slow consumer always gets the latest
// frame and never a backlog.
class LatestFrame {
 public:
  void Publish(FrameRef frame);
  // Waits until a frame with seq > after_seq is available. Returns nothing on
  // timeout or after Close().
  [[nodiscard]] std::optional<FrameRef> WaitNewer(
      std::uint64_t after_seq, std::chrono::microseconds timeout);
  void Close();

 private:
  std::mutex mutex_;
  std::condition_variable cv_;
  std::optional<FrameRef> latest_;
  bool closed_ = false;
};

}  // namespace jetson_camera

#endif  // JETSON_CAMERA_SRC_FRAME_H_
