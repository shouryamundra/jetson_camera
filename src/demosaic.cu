// Purpose: Implement the Bayer demosaic kernel.
// Paradigm: Shared-memory tiled stencil kernel, one thread per output pixel.
// Role: Converts raw sensor data to BGRA on the GPU so the CPU never touches pixels.

#include "cuda_error.hpp"
#include "demosaic.hpp"

namespace io
{
namespace
{
constexpr int kBlockX = 32;
constexpr int kBlockY = 8;
constexpr int kTileX = kBlockX + 2;  // one-pixel halo on every side
constexpr int kTileY = kBlockY + 2;

// Reads the neighbours of the thread's pixel from the shared tile.
struct TileSample
{
  const std::uint8_t (*tile)[kTileX];
  int x;
  int y;

  __host__ __device__ int operator()(const int dx, const int dy) const
  {
    return tile[y + dy][x + dx];
  }
};

__global__ void demosaic_kernel(
  const std::uint8_t * __restrict__ raw, const int width, const int height, const Bayer pattern,
  uchar4 * __restrict__ out)
{
  __shared__ std::uint8_t tile[kTileY][kTileX];

  // Sensor coordinates of the tile's top-left sample; the halo starts one pixel before the block.
  const int origin_x = static_cast<int>(blockIdx.x) * kBlockX - 1;
  const int origin_y = static_cast<int>(blockIdx.y) * kBlockY - 1;

  // Each raw byte is fetched from global memory once per block, then the stencil reads shared
  // memory. Every thread takes part in the load and the barrier, including those past the edge.
  const int thread = static_cast<int>(threadIdx.y) * kBlockX + static_cast<int>(threadIdx.x);
  for (int i = thread; i < kTileX * kTileY; i += kBlockX * kBlockY) {
    const int ty = i / kTileX;
    const int tx = i - ty * kTileX;
    tile[ty][tx] = raw[reflect(origin_y + ty, height) * width + reflect(origin_x + tx, width)];
  }
  __syncthreads();

  const int x = origin_x + 1 + static_cast<int>(threadIdx.x);
  const int y = origin_y + 1 + static_cast<int>(threadIdx.y);
  if (x >= width || y >= height) return;

  const TileSample sample{
    tile, static_cast<int>(threadIdx.x) + 1, static_cast<int>(threadIdx.y) + 1};
  const Bgr px = demosaic(pattern, x, y, sample);

  // One 32-bit store per thread; consecutive threads write consecutive pixels.
  out[y * width + x] = make_uchar4(px.b, px.g, px.r, 255);
}

}  // namespace

void launch_demosaic(
  const std::uint8_t * const raw, const int width, const int height, const Bayer & pattern,
  uchar4 * const out, const cudaStream_t stream)
{
  const dim3 block(kBlockX, kBlockY);
  const dim3 grid(
    static_cast<unsigned>((width + kBlockX - 1) / kBlockX),
    static_cast<unsigned>((height + kBlockY - 1) / kBlockY));
  demosaic_kernel<<<grid, block, 0, stream>>>(raw, width, height, pattern, out);
  cuda::check(cudaGetLastError(), "demosaic launch");
}

}  // namespace io
