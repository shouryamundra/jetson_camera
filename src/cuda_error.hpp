// Purpose: Turn CUDA runtime error codes into exceptions.
// Paradigm: Header-only checked-call helper kept free of C++20 so nvcc can include it.
// Role: Makes every CUDA failure loud and carries the failing call's name.

#ifndef IO__CUDA_ERROR_HPP
#define IO__CUDA_ERROR_HPP

#include <cuda_runtime.h>

#include <stdexcept>
#include <string>

namespace io::cuda
{
inline void check(const cudaError_t status, const char * const what)
{
  if (status != cudaSuccess) {
    throw std::runtime_error(std::string(what) + ": " + cudaGetErrorString(status));
  }
}

}  // namespace io::cuda

#endif  // IO__CUDA_ERROR_HPP
