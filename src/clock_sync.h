// Maps camera-clock timestamps onto the host clock without a hardware sync.
//
// arrival = stamp + offset + fixed_latency + jitter, with jitter >= 0, so the
// minimum of (arrival - stamp) over a sliding window tracks offset +
// fixed_latency, and drift between the clocks (tens of ppm) is followed as old
// samples leave the window. A monotonic deque keeps the window minimum in O(1)
// amortised per frame. Measured on the MV-CS016-10UC: the stamp marks
// exposure start and arrival jitter is ~30 us.

#ifndef JETSON_CAMERA_SRC_CLOCK_SYNC_H_
#define JETSON_CAMERA_SRC_CLOCK_SYNC_H_

#include <cstdint>
#include <deque>

namespace jetson_camera {

class ClockSync {
 public:
  // window_us: how long a minimum is remembered (shorter follows drift faster).
  // reset_us: a jump bigger than this is a clock restart if it persists for
  // reset_frames consecutive frames; fewer is one badly delayed frame.
  explicit ClockSync(std::int64_t window_us = 2'000'000,
                     std::int64_t reset_us = 200'000, int reset_frames = 5)
      : window_us_(window_us),
        reset_us_(reset_us),
        reset_frames_(reset_frames) {}

  // Feeds one frame; returns the host time (clock of arrival_us) of camera
  // time stamp_us, minus latency_us, the fixed delay the envelope cannot see.
  std::int64_t ToHost(std::int64_t stamp_us, std::int64_t arrival_us,
                      std::int64_t latency_us) {
    const std::int64_t d = arrival_us - stamp_us;
    const bool jump = !window_.empty() && (d - window_.front().d > reset_us_ ||
                                           window_.front().d - d > reset_us_);
    jumps_ = jump ? jumps_ + 1 : 0;
    if (jump && d > window_.front().d) {
      if (jumps_ < reset_frames_)
        return stamp_us + window_.front().d - latency_us;
      window_.clear();
    }
    if (jump && d < window_.front().d) window_.clear();
    // A sample at or above the new one can never be the minimum again.
    while (!window_.empty() && window_.back().d >= d) window_.pop_back();
    window_.push_back({arrival_us, d});
    while (window_.front().t < arrival_us - window_us_) window_.pop_front();
    return stamp_us + window_.front().d - latency_us;
  }

  // Forgets all history (the camera reconnected and its clock restarted).
  void Reset() {
    window_.clear();
    jumps_ = 0;
  }

  // Current envelope (offset + fixed latency), microseconds.
  [[nodiscard]] std::int64_t min_delay() const {
    return window_.empty() ? 0 : window_.front().d;
  }

 private:
  struct Sample {
    std::int64_t t;
    std::int64_t d;
  };
  std::int64_t window_us_;
  std::int64_t reset_us_;
  int reset_frames_;
  int jumps_ = 0;
  std::deque<Sample> window_;  // d strictly increasing from front to back.
};

}  // namespace jetson_camera

#endif  // JETSON_CAMERA_SRC_CLOCK_SYNC_H_
