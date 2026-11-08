// CUDA error checking and RAII ownership of CUDA memory, streams and events.
//
// Two error styles: setup code uses JC_CUDA_CHECK, which throws with the
// failing call and its location; hot paths test the returned cudaError_t and
// count failures instead of throwing.

#ifndef JETSON_CAMERA_SRC_CUDA_UTIL_H_
#define JETSON_CAMERA_SRC_CUDA_UTIL_H_

#include <cuda_runtime.h>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>

namespace jetson_camera {
namespace cuda {

// Throws std::runtime_error naming the call and source location on failure.
inline void Check(cudaError_t status, const char* what, const char* file,
                  int line) {
  if (status == cudaSuccess) return;
  throw std::runtime_error(std::string(file) + ":" + std::to_string(line) +
                           ": " + what + ": " + cudaGetErrorString(status));
}

#define JC_CUDA_CHECK(call) \
  ::jetson_camera::cuda::Check((call), #call, __FILE__, __LINE__)

struct HostFree {
  void operator()(void* p) const noexcept { cudaFreeHost(p); }
};
struct DeviceFree {
  void operator()(void* p) const noexcept { cudaFree(p); }
};
struct StreamDestroy {
  void operator()(cudaStream_t s) const noexcept { cudaStreamDestroy(s); }
};
struct EventDestroy {
  void operator()(cudaEvent_t e) const noexcept { cudaEventDestroy(e); }
};

// Page-locked memory shared by CPU and GPU. Orin has unified addressing, so
// the same pointer works in host code and in kernels, with no copy.
template <typename T>
using Pinned = std::unique_ptr<T[], HostFree>;
// GPU memory (same DRAM on Orin, but GPU-cached): for data only the GPU reads.
template <typename T>
using Device = std::unique_ptr<T[], DeviceFree>;
using Stream = std::unique_ptr<CUstream_st, StreamDestroy>;
using Event = std::unique_ptr<CUevent_st, EventDestroy>;

template <typename T>
[[nodiscard]] Pinned<T> MakePinned(std::size_t count) {
  void* p = nullptr;
  JC_CUDA_CHECK(cudaHostAlloc(&p, count * sizeof(T), cudaHostAllocMapped));
  return Pinned<T>(static_cast<T*>(p));
}

template <typename T>
[[nodiscard]] Device<T> MakeDevice(std::size_t count) {
  void* p = nullptr;
  JC_CUDA_CHECK(cudaMalloc(&p, count * sizeof(T)));
  return Device<T>(static_cast<T*>(p));
}

// The most urgent stream priority this device supports (numerically lowest).
[[nodiscard]] inline int HighestStreamPriority() {
  int least = 0;
  int greatest = 0;
  JC_CUDA_CHECK(cudaDeviceGetStreamPriorityRange(&least, &greatest));
  return greatest;
}

// A non-blocking stream: never implicitly synchronises with the legacy
// default stream.
[[nodiscard]] inline Stream MakeStream(int priority = 0) {
  cudaStream_t s = nullptr;
  JC_CUDA_CHECK(
      cudaStreamCreateWithPriority(&s, cudaStreamNonBlocking, priority));
  return Stream(s);
}

[[nodiscard]] inline Event MakeEvent(unsigned flags = cudaEventDefault) {
  cudaEvent_t e = nullptr;
  JC_CUDA_CHECK(cudaEventCreateWithFlags(&e, flags));
  return Event(e);
}

}  // namespace cuda
}  // namespace jetson_camera

#endif  // JETSON_CAMERA_SRC_CUDA_UTIL_H_
