// Purpose: Implement the fused Bayer demosaic and crop kernel.
// Paradigm: Shared-memory tiled stencil kernel, one thread per output pixel.
// Role: Converts raw sensor data to BGRA on the GPU so the CPU never touches pixels.

#include "demosaic.hpp"

namespace io
{
namespace
{
constexpr int kBlockX = 32;
constexpr int kBlockY = 8;
constexpr int kTileX = kBlockX + 2;  // one-pixel halo on every side
constexpr int kTileY = kBlockY + 2;

// Reads neighbours of the thread's pixel from the shared tile.
struct TileSample
{
  const uint8_t (*tile)[kTileX];
  int x;
  int y;
  __host__ __device__ int operator()(int dx, int dy) const { return tile[y + dy][x + dx]; }
};

__global__ void demosaic_kernel(
  const uint8_t * __restrict__ raw, int raw_width, int raw_height, Bayer pattern, Roi roi,
  uchar4 * __restrict__ out)
{
  __shared__ uint8_t tile[kTileY][kTileX];

  // Absolute sensor coordinates of the tile's top-left sample (the halo starts one pixel before).
  const int origin_x = roi.x + blockIdx.x * kBlockX - 1;
  const int origin_y = roi.y + blockIdx.y * kBlockY - 1;

  // Each raw byte is fetched from global memory once per block; the stencil then reads shared
  // memory. Every thread takes part in the load and the barrier, including those past the roi.
  const int thread = threadIdx.y * kBlockX + threadIdx.x;
  for (int i = thread; i < kTileX * kTileY; i += kBlockX * kBlockY) {
    const int ty = i / kTileX;
    const int tx = i - ty * kTileX;
    tile[ty][tx] =
      raw[reflect(origin_y + ty, raw_height) * raw_width + reflect(origin_x + tx, raw_width)];
  }
  __syncthreads();

  const int out_x = blockIdx.x * kBlockX + threadIdx.x;
  const int out_y = blockIdx.y * kBlockY + threadIdx.y;
  if (out_x >= roi.width || out_y >= roi.height) return;

  const TileSample sample{tile, int(threadIdx.x) + 1, int(threadIdx.y) + 1};
  const Bgr px = demosaic(pattern, roi.x + out_x, roi.y + out_y, sample);

  // One 32-bit store per thread; consecutive threads write consecutive pixels.
  out[out_y * roi.width + out_x] = make_uchar4(px.b, px.g, px.r, 255);
}

}  // namespace

cudaError_t launch_demosaic(
  const uint8_t * raw, int raw_width, int raw_height, const Bayer & pattern, const Roi & roi,
  uchar4 * out, cudaStream_t stream)
{
  const dim3 block(kBlockX, kBlockY);
  const dim3 grid((roi.width + kBlockX - 1) / kBlockX, (roi.height + kBlockY - 1) / kBlockY);
  demosaic_kernel<<<grid, block, 0, stream>>>(raw, raw_width, raw_height, pattern, roi, out);
  return cudaGetLastError();
}

}  // namespace io
