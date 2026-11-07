// Purpose: Hand the newest value from one producer thread to one consumer thread.
// Paradigm: Lock-free triple buffer; one atomic word, no mutex.
// Role: Lets USB capture run at full rate while a slower consumer always reads the freshest frame.

#ifndef IO__FRAME_MAILBOX_HPP
#define IO__FRAME_MAILBOX_HPP

#include <array>
#include <atomic>
#include <concepts>
#include <cstdint>

namespace io
{
// Three slots rotate between the producer (writing), the consumer (reading) and the middle.
// The atomic word holds the middle slot's index plus two flags. Each side only ever exchanges
// its own slot with the middle one, so neither side waits for the other, a published frame that
// is never read is simply overwritten, and acquire() returns the newest frame published.
template <typename T>
class FrameMailbox
{
public:
  FrameMailbox()
    requires std::default_initializable<T>
  = default;

  // Builds each slot by calling `make()`, for slots that are not default constructible.
  template <std::invocable<> Make>
  explicit FrameMailbox(Make && make) : slots_{make(), make(), make()}
  {
  }

  // Slot the producer may fill. Call publish() when it is complete.
  [[nodiscard]] T & write_slot() noexcept { return slots_[write_]; }

  void publish() noexcept
  {
    const std::uint32_t previous = middle_.exchange(write_ | kFresh, std::memory_order_acq_rel);
    write_ = previous & kIndex;
    middle_.notify_one();
  }

  // Newest published frame, sleeping until one exists. The slot stays valid until the next
  // acquire(). Returns nullptr once closed.
  [[nodiscard]] T * acquire() noexcept
  {
    std::uint32_t state = middle_.load(std::memory_order_acquire);
    while (!(state & (kFresh | kClosed))) {
      middle_.wait(state, std::memory_order_acquire);
      state = middle_.load(std::memory_order_acquire);
    }
    if (state & kClosed) return nullptr;
    read_ = middle_.exchange(read_, std::memory_order_acq_rel) & kIndex;
    return &slots_[read_];
  }

  // Wakes a blocked acquire(), which returns nullptr. Call only after the producer has stopped.
  void close() noexcept
  {
    middle_.fetch_or(kClosed, std::memory_order_acq_rel);
    middle_.notify_all();
  }

  // Forget all frames and reopen. Call only while neither side is running.
  void reset() noexcept
  {
    write_ = 0;
    read_ = 1;
    middle_.store(2, std::memory_order_release);
  }

private:
  static constexpr std::uint32_t kIndex = 3;
  static constexpr std::uint32_t kFresh = 4;
  static constexpr std::uint32_t kClosed = 8;

  std::array<T, 3> slots_;
  std::uint32_t write_ = 0;  // owned by the producer
  std::uint32_t read_ = 1;   // owned by the consumer
  std::atomic<std::uint32_t> middle_{2};
};

}  // namespace io

#endif  // IO__FRAME_MAILBOX_HPP
