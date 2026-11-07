// Purpose: Own CUDA resources with unique_ptr so they are released on every exit path.
// Paradigm: Header-only RAII wrappers over the CUDA runtime API.
// Role: Keeps buffers, streams and events leak-free, including when a constructor throws.

#ifndef IO__CUDA_RAII_HPP
#define IO__CUDA_RAII_HPP

#include <cuda_runtime.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

#include "cuda_error.hpp"

namespace io::cuda
{
struct DeviceFree
{
  void operator()(void * p) const noexcept { cudaFree(p); }
};

struct HostFree
{
  void operator()(void * p) const noexcept { cudaFreeHost(p); }
};

struct StreamDestroy
{
  void operator()(CUstream_st * s) const noexcept { cudaStreamDestroy(s); }
};

template <typename T>
using DeviceBuffer = std::unique_ptr<T[], DeviceFree>;
using Stream = std::unique_ptr<CUstream_st, StreamDestroy>;

template <typename T>
[[nodiscard]] DeviceBuffer<T> make_device_buffer(const std::size_t count)
{
  void * p = nullptr;
  check(cudaMalloc(&p, count * sizeof(T)), "cudaMalloc");
  return DeviceBuffer<T>(static_cast<T *>(p));
}

[[nodiscard]] inline Stream make_stream()
{
  cudaStream_t s = nullptr;
  check(cudaStreamCreateWithFlags(&s, cudaStreamNonBlocking), "cudaStreamCreate");
  return Stream(s);
}

// Page-locked host memory that the GPU can also address. On Jetson the CPU and GPU share one
// DRAM, so both sides read and write the same bytes with no copy.
class PinnedBuffer
{
public:
  explicit PinnedBuffer(const std::size_t bytes) : size_(bytes)
  {
    void * host = nullptr;
    check(cudaHostAlloc(&host, bytes, cudaHostAllocMapped), "cudaHostAlloc");
    host_.reset(static_cast<std::uint8_t *>(host));
    void * device = nullptr;
    check(cudaHostGetDevicePointer(&device, host, 0), "cudaHostGetDevicePointer");
    device_ = static_cast<std::uint8_t *>(device);
  }

  [[nodiscard]] std::uint8_t * host() const noexcept { return host_.get(); }
  [[nodiscard]] std::uint8_t * device() const noexcept { return device_; }
  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] std::span<std::uint8_t> span() const noexcept { return {host_.get(), size_}; }

private:
  std::unique_ptr<std::uint8_t[], HostFree> host_;
  std::uint8_t * device_ = nullptr;
  std::size_t size_ = 0;
};

// Makes stream synchronisation spin instead of sleeping, which wakes fastest and costs one busy
// core while waiting. Construct it before anything creates the CUDA context; if a context already
// exists its settings are kept.
struct SpinScheduling
{
  SpinScheduling()
  {
    const cudaError_t status = cudaSetDeviceFlags(cudaDeviceScheduleSpin | cudaDeviceMapHost);
    if (status == cudaErrorSetOnActiveProcess) {
      static_cast<void>(cudaGetLastError());
      return;
    }
    check(status, "cudaSetDeviceFlags");
  }
};

}  // namespace io::cuda

#endif  // IO__CUDA_RAII_HPP
