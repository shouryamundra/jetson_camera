// Purpose: Declare the GPU Bayer-to-BGRA conversion with a fused region-of-interest crop.
// Paradigm: Thin host launcher over a single CUDA kernel.
// Role: Turns raw sensor frames into the pixels that downstream GPU work consumes.

#ifndef IO__DEMOSAIC_HPP
#define IO__DEMOSAIC_HPP

#include <cuda_runtime.h>

#include <cstdint>

#include "bayer.hpp"

namespace io
{
struct Roi
{
  int x = 0;
  int y = 0;
  int width = 0;  // 0 selects the full frame
  int height = 0;
};

// Converts the `roi` region of a raw 8-bit Bayer frame (`raw_width` x `raw_height`, tightly
// packed, device-accessible) to BGRA8 in `out` (`roi.width` x `roi.height`, tightly packed).
// Only the roi pixels are computed. Asynchronous on `stream`; the caller keeps `raw` and `out`
// alive until the stream passes this point. Returns the launch status.
cudaError_t launch_demosaic(
  const uint8_t * raw, int raw_width, int raw_height, const Bayer & pattern, const Roi & roi,
  uchar4 * out, cudaStream_t stream);

}  // namespace io

#endif  // IO__DEMOSAIC_HPP
