// Purpose: Declare the GPU Bayer-to-BGRA conversion.
// Paradigm: Thin host launcher over a single CUDA kernel.
// Role: Turns raw sensor frames into the pixels that downstream GPU work consumes.

#ifndef IO__DEMOSAIC_HPP
#define IO__DEMOSAIC_HPP

#include <cuda_runtime.h>

#include <cstdint>

#include "bayer.hpp"

namespace io
{
// Converts a raw 8-bit Bayer frame (`width` x `height`, tightly packed, device-accessible) to
// BGRA8 in `out` (same size, tightly packed). Asynchronous on `stream`; the caller keeps `raw`
// and `out` alive until the stream passes this point. Throws if the launch fails.
void launch_demosaic(
  const std::uint8_t * raw, int width, int height, const Bayer & pattern, uchar4 * out,
  cudaStream_t stream);

}  // namespace io

#endif  // IO__DEMOSAIC_HPP
