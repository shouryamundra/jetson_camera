// Purpose: Implement the Bayer demosaic kernel.
// Paradigm: One thread per output pixel; neighbours come through the cache.
// Role: Converts raw sensor data to BGRA on the GPU so the CPU never touches pixels.

#include "cuda.hpp"
#include "demosaic.hpp"

namespace io
{
namespace
{
__global__ void demosaic_kernel(
  const std::uint8_t * __restrict__ raw, const int width, const int height, const Bayer pattern,
  uchar4 * __restrict__ out)
{
  const int x = static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);
  const int y = static_cast<int>(blockIdx.y * blockDim.y + threadIdx.y);
  if (x >= width || y >= height) return;

  const auto at = [&](const int dx, const int dy) {
    return static_cast<int>(raw[reflect(y + dy, height) * width + reflect(x + dx, width)]);
  };
  const Bgr px = demosaic(pattern, x, y, at);
  out[y * width + x] = make_uchar4(px.b, px.g, px.r, 255);
}

}  // namespace

void launch_demosaic(
  const std::uint8_t * const raw, const int width, const int height, const Bayer & pattern,
  uchar4 * const out, const cudaStream_t stream)
{
  const dim3 block(32, 8);
  const dim3 grid((width + 31) / 32, (height + 7) / 8);
  demosaic_kernel<<<grid, block, 0, stream>>>(raw, width, height, pattern, out);
  cuda::check(cudaGetLastError(), "demosaic launch");
}

}  // namespace io
