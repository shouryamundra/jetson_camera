// Frame pool, frame handles and the latest-frame mailbox: two big CUDA
// allocations carved into fixed slots, atomic refcounts, mutex + condvar.

#include "src/frame.h"

#include <cstdio>
#include <utility>

#include "src/cuda_util.h"

namespace jetson_camera {
namespace {

constexpr std::size_t kAlign = 256;  // Every plane aligned for vector access.

std::size_t RoundUp(std::size_t n) {
  return (n + kAlign - 1) / kAlign * kAlign;
}

}  // namespace

FrameRef::FrameRef(const FrameRef& other) noexcept
    : pool_(other.pool_), index_(other.index_) {
  if (pool_) pool_->slots_[index_].refs.fetch_add(1, std::memory_order_relaxed);
}

FrameRef::~FrameRef() {
  if (pool_ &&
      pool_->slots_[index_].refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    pool_->Release(index_);
  }
}

const FrameInfo& FrameRef::info() const { return pool_->slots_[index_].info; }
const FrameBuffers& FrameRef::buffers() const {
  return pool_->slots_[index_].buffers;
}

FramePool::FramePool(std::size_t count, const Layout& layout)
    : layout_(layout), count_(count), slots_(std::make_unique<Slot[]>(count)) {
  const auto pixels = static_cast<std::size_t>(layout.width) *
                      static_cast<std::size_t>(layout.height);
  const std::size_t tensor_bytes =
      layout.tensor ? RoundUp(3 * pixels * (layout.tensor_fp16 ? 2 : 4)) : 0;
  const std::size_t bgr_bytes = layout.bgr ? RoundUp(3 * pixels) : 0;
  const std::size_t nv12_bytes = layout.nv12 ? RoundUp(pixels * 3 / 2) : 0;
  const std::size_t pinned_bytes = bgr_bytes + nv12_bytes;

  if (tensor_bytes)
    JC_CUDA_CHECK(cudaMalloc(&device_block_, tensor_bytes * count));
  if (pinned_bytes) {
    JC_CUDA_CHECK(cudaHostAlloc(&pinned_block_, pinned_bytes * count,
                                cudaHostAllocMapped));
  }
  auto* device = static_cast<std::uint8_t*>(device_block_);
  auto* pinned = static_cast<std::uint8_t*>(pinned_block_);
  free_.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    FrameBuffers& b = slots_[i].buffers;
    if (tensor_bytes) b.tensor = device + i * tensor_bytes;
    std::uint8_t* p = pinned ? pinned + i * pinned_bytes : nullptr;
    if (bgr_bytes) b.bgr = p;
    if (nv12_bytes) b.nv12 = p + bgr_bytes;
    free_.push_back(static_cast<std::uint32_t>(count - 1 - i));
  }
}

FramePool::~FramePool() {
  // Every FrameRef should be gone by now. If one is not, leaking is the safe
  // choice: freeing would let a consumer (or the GPU) touch freed memory.
  {
    std::lock_guard lock(free_mutex_);
    if (free_.size() != count_) {
      std::fprintf(stderr,
                   "FramePool: %zu frames still referenced at shutdown; "
                   "leaking pool memory\n",
                   count_ - free_.size());
      return;
    }
  }
  if (device_block_) cudaFree(device_block_);
  if (pinned_block_) cudaFreeHost(pinned_block_);
}

std::optional<FrameRef> FramePool::Acquire() noexcept {
  std::uint32_t index;
  {
    std::lock_guard lock(free_mutex_);
    if (free_.empty()) return std::nullopt;
    index = free_.back();
    free_.pop_back();
  }
  slots_[index].refs.store(1, std::memory_order_relaxed);
  slots_[index].info = {};
  return FrameRef(this, index);
}

std::size_t FramePool::free_count() const {
  std::lock_guard lock(free_mutex_);
  return free_.size();
}

void FramePool::Release(std::uint32_t index) noexcept {
  std::lock_guard lock(free_mutex_);
  free_.push_back(index);  // Capacity reserved: never allocates.
}

void LatestFrame::Publish(FrameRef frame) {
  std::optional<FrameRef> previous;  // Released outside the lock.
  {
    std::lock_guard lock(mutex_);
    previous = std::exchange(latest_, std::move(frame));
  }
  cv_.notify_all();
}

std::optional<FrameRef> LatestFrame::WaitNewer(
    std::uint64_t after_seq, std::chrono::microseconds timeout) {
  std::unique_lock lock(mutex_);
  const bool ready = cv_.wait_for(lock, timeout, [&] {
    return closed_ || (latest_ && latest_->info().seq > after_seq);
  });
  if (!ready || closed_) return std::nullopt;
  return latest_;  // A copy: one more reference.
}

void LatestFrame::Close() {
  {
    std::lock_guard lock(mutex_);
    closed_ = true;
    latest_.reset();
  }
  cv_.notify_all();
}

}  // namespace jetson_camera
