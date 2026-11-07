// Purpose: Checked CUDA calls and RAII ownership of pinned memory and streams.
// Paradigm: Header-only unique_ptr wrappers over the CUDA runtime API.
// Role: Makes CUDA failures loud and CUDA resources leak-free.

#ifndef IO__CUDA_HPP
#define IO__CUDA_HPP

#include <cuda_runtime.h>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>

namespace io::cuda
{
inline void check(const cudaError_t status, const char * const what)
{
  if (status != cudaSuccess)
    throw std::runtime_error(std::string(what) + ": " + cudaGetErrorString(status));
}

struct PinnedFree
{
  void operator()(void * p) const noexcept { cudaFreeHost(p); }
};

struct StreamDestroy
{
  void operator()(cudaStream_t s) const noexcept { cudaStreamDestroy(s); }
};

// Page-locked memory the CPU and GPU share. Orin has unified addressing, so the same pointer
// works in host code and in kernels, with no copy between them.
template <typename T>
using Pinned = std::unique_ptr<T[], PinnedFree>;

using Stream = std::unique_ptr<CUstream_st, StreamDestroy>;

template <typename T>
[[nodiscard]] Pinned<T> make_pinned(const std::size_t count)
{
  void * p = nullptr;
  check(cudaHostAlloc(&p, count * sizeof(T), cudaHostAllocMapped), "cudaHostAlloc");
  return Pinned<T>(static_cast<T *>(p));
}

[[nodiscard]] inline Stream make_stream()
{
  cudaStream_t s = nullptr;
  check(cudaStreamCreateWithFlags(&s, cudaStreamNonBlocking), "cudaStreamCreate");
  return Stream(s);
}

}  // namespace io::cuda

#endif  // IO__CUDA_HPP
