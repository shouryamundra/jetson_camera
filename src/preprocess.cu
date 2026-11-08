// The fused Bayer -> {tensor, BGR, NV12} kernel.
//
// One thread per 2x2 output block: load the 5x5 Bayer window once into
// registers (read-only cache), demosaic the 3x3 source pixels it covers,
// area-average down by 1.5, write every enabled output. Replaces separate
// demosaic, resize, normalise, layout-change and colour-conversion passes.

#include <cuda_fp16.h>

#include "src/preprocess.h"

namespace jetson_camera {
namespace {

struct Rgb {
  int r, g, b;
};

// 1.5x area weights (x2): output 0 covers source [0, 1.5) -> {2, 1, 0};
// output 1 covers [1.5, 3) -> {0, 1, 2}. Products over both axes sum to 9.
__device__ constexpr int kWeight[2][3] = {{2, 1, 0}, {0, 1, 2}};

// Each thread owns a 2x2 output block = 3x3 source pixels; its demosaic needs
// a 5x5 raw window, and neighbouring windows overlap (~2.8 reads per byte).
// Two load strategies (PreprocessParams::shared_tile): direct read-only-cache
// loads per thread, or a cooperative shared-memory tile per block that reads
// every raw byte once. Which is faster is measured, not assumed
// (preprocess_test prints both).
constexpr int kBlockX = 32;
constexpr int kBlockY = 4;
constexpr int kTileW = kBlockX * 3 + 2;  // 98 raw columns per block.
constexpr int kTileH = kBlockY * 3 + 2;  // 14 raw rows per block.

__device__ __forceinline__ unsigned char Luma709(const Rgb& c) {
  return static_cast<unsigned char>(
      ((47 * c.r + 157 * c.g + 16 * c.b + 128) >> 8) + 16);
}

template <bool kFp16, bool kTiled>
__global__ void __launch_bounds__(kBlockX* kBlockY)
    PreprocessKernel(const PreprocessParams p, const FrameBuffers out) {
  const std::uint8_t* __restrict__ raw = p.raw;
  const auto pitch = static_cast<std::size_t>(p.raw_width);
  const int bx = static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);
  const int by = static_cast<int>(blockIdx.y * blockDim.y + threadIdx.y);
  // Source 3x3 block at (sx, sy); its demosaic reads rows/cols sx-1 .. sx+3.
  const int sx = p.x0 + bx * 3;
  const int sy = p.y0 + by * 3;
  std::uint8_t win[5][5];

  if constexpr (kTiled) {
    __shared__ std::uint8_t tile[kTileH][kTileW];
    // Cooperative row-wise load, coalesced along x, no index division; all
    // threads take part before any early return.
    const int tile_x0 = p.x0 + static_cast<int>(blockIdx.x) * kBlockX * 3 - 1;
    const int tile_y0 = p.y0 + static_cast<int>(blockIdx.y) * kBlockY * 3 - 1;
    for (int r = static_cast<int>(threadIdx.y); r < kTileH; r += kBlockY) {
      const std::uint8_t* row =
          raw +
          static_cast<std::size_t>(Reflect(tile_y0 + r, p.raw_height)) * pitch;
      for (int c = static_cast<int>(threadIdx.x); c < kTileW; c += kBlockX) {
        tile[r][c] = __ldg(row + Reflect(tile_x0 + c, p.raw_width));
      }
    }
    __syncthreads();
    if (bx * 2 >= p.out_width || by * 2 >= p.out_height) return;
    const int wx = static_cast<int>(threadIdx.x) * 3;
    const int wy = static_cast<int>(threadIdx.y) * 3;
#pragma unroll
    for (int j = 0; j < 5; ++j) {
#pragma unroll
      for (int i = 0; i < 5; ++i) win[j][i] = tile[wy + j][wx + i];
    }
  } else {
    if (bx * 2 >= p.out_width || by * 2 >= p.out_height) return;
#pragma unroll
    for (int j = 0; j < 5; ++j) {
      const std::uint8_t* row =
          raw +
          static_cast<std::size_t>(Reflect(sy - 1 + j, p.raw_height)) * pitch;
#pragma unroll
      for (int i = 0; i < 5; ++i) {
        win[j][i] = __ldg(row + Reflect(sx - 1 + i, p.raw_width));
      }
    }
  }

  Rgb src[3][3];
#pragma unroll
  for (int j = 0; j < 3; ++j) {
#pragma unroll
    for (int i = 0; i < 3; ++i) {
      const auto at = [&](int dx, int dy) {
        return static_cast<int>(win[1 + j + dy][1 + i + dx]);
      };
      const Bgr c = Demosaic(p.pattern, sx + i, sy + j, at);
      src[j][i] = {c.r, c.g, c.b};
    }
  }

  Rgb o[2][2];
#pragma unroll
  for (int oy = 0; oy < 2; ++oy) {
#pragma unroll
    for (int ox = 0; ox < 2; ++ox) {
      int r = 0, g = 0, b = 0;
#pragma unroll
      for (int j = 0; j < 3; ++j) {
#pragma unroll
        for (int i = 0; i < 3; ++i) {
          const int w = kWeight[oy][j] * kWeight[ox][i];
          r += w * src[j][i].r;
          g += w * src[j][i].g;
          b += w * src[j][i].b;
        }
      }
      o[oy][ox] = {(r + 4) / 9, (g + 4) / 9, (b + 4) / 9};
    }
  }

  const int x = bx * 2;
  const int y = by * 2;
  const auto width = static_cast<std::size_t>(p.out_width);
  const std::size_t plane = width * static_cast<std::size_t>(p.out_height);

  if (out.tensor != nullptr) {
    constexpr float kScale = 1.0f / 255.0f;
#pragma unroll
    for (int oy = 0; oy < 2; ++oy) {
      const std::size_t idx = static_cast<std::size_t>(y + oy) * width + x;
      const Rgb& a = o[oy][0];
      const Rgb& c = o[oy][1];
      const int first[2] = {p.tensor_rgb ? a.r : a.b, p.tensor_rgb ? c.r : c.b};
      const int last[2] = {p.tensor_rgb ? a.b : a.r, p.tensor_rgb ? c.b : c.r};
      // x is even, so each pair is one aligned 4- or 8-byte store.
      if constexpr (kFp16) {
        auto* t = static_cast<__half2*>(out.tensor);
        t[idx / 2] = __floats2half2_rn(first[0] * kScale, first[1] * kScale);
        t[(plane + idx) / 2] = __floats2half2_rn(a.g * kScale, c.g * kScale);
        t[(2 * plane + idx) / 2] =
            __floats2half2_rn(last[0] * kScale, last[1] * kScale);
      } else {
        auto* t = static_cast<float2*>(out.tensor);
        t[idx / 2] = make_float2(first[0] * kScale, first[1] * kScale);
        t[(plane + idx) / 2] = make_float2(a.g * kScale, c.g * kScale);
        t[(2 * plane + idx) / 2] =
            make_float2(last[0] * kScale, last[1] * kScale);
      }
    }
  }

  if (out.bgr != nullptr) {
#pragma unroll
    for (int oy = 0; oy < 2; ++oy) {
      std::uint8_t* d =
          out.bgr + (static_cast<std::size_t>(y + oy) * width + x) * 3;
#pragma unroll
      for (int ox = 0; ox < 2; ++ox) {
        d[ox * 3 + 0] = static_cast<std::uint8_t>(o[oy][ox].b);
        d[ox * 3 + 1] = static_cast<std::uint8_t>(o[oy][ox].g);
        d[ox * 3 + 2] = static_cast<std::uint8_t>(o[oy][ox].r);
      }
    }
  }

  if (out.nv12 != nullptr) {
    // BT.709 limited range, integer coefficients (x256); one chroma pair per
    // 2x2 block, from the block's average colour.
    int rs = 0, gs = 0, bs = 0;
#pragma unroll
    for (int oy = 0; oy < 2; ++oy) {
      uchar2 luma;
      luma.x = Luma709(o[oy][0]);
      luma.y = Luma709(o[oy][1]);
      *reinterpret_cast<uchar2*>(
          out.nv12 + static_cast<std::size_t>(y + oy) * width + x) = luma;
      rs += o[oy][0].r + o[oy][1].r;
      gs += o[oy][0].g + o[oy][1].g;
      bs += o[oy][0].b + o[oy][1].b;
    }
    rs = (rs + 2) >> 2;
    gs = (gs + 2) >> 2;
    bs = (bs + 2) >> 2;
    uchar2 chroma;
    chroma.x = static_cast<unsigned char>(
        ((-26 * rs - 86 * gs + 112 * bs + 128) >> 8) + 128);
    chroma.y = static_cast<unsigned char>(
        ((112 * rs - 102 * gs - 10 * bs + 128) >> 8) + 128);
    *reinterpret_cast<uchar2*>(
        out.nv12 + plane + static_cast<std::size_t>(by) * width + x) = chroma;
  }
}

}  // namespace

cudaError_t LaunchPreprocess(const PreprocessParams& p, const FrameBuffers& out,
                             cudaStream_t stream) noexcept {
  const dim3 block(kBlockX, kBlockY);
  const dim3 grid(
      static_cast<unsigned>((p.out_width / 2 + kBlockX - 1) / kBlockX),
      static_cast<unsigned>((p.out_height / 2 + kBlockY - 1) / kBlockY));
  if (p.tensor_fp16 && p.shared_tile) {
    PreprocessKernel<true, true><<<grid, block, 0, stream>>>(p, out);
  } else if (p.tensor_fp16) {
    PreprocessKernel<true, false><<<grid, block, 0, stream>>>(p, out);
  } else if (p.shared_tile) {
    PreprocessKernel<false, true><<<grid, block, 0, stream>>>(p, out);
  } else {
    PreprocessKernel<false, false><<<grid, block, 0, stream>>>(p, out);
  }
  return cudaGetLastError();
}

}  // namespace jetson_camera
