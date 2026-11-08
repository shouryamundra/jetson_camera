// Checks the fused GPU kernel against a straightforward CPU reference built on
// the same bayer.h math, and times it reading from device memory and from
// mapped host memory (how real SDK buffers are read). Needs a GPU, no camera.

#include <cuda_fp16.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

#include "src/cuda_util.h"
#include "src/frame.h"
#include "src/preprocess.h"

namespace {

namespace jc = jetson_camera;

struct Rgb {
  int r, g, b;
};

constexpr int kRawW = 1440;
constexpr int kRawH = 812;
constexpr int kOutW = 960;
constexpr int kOutH = 540;

// CPU reference: demosaic each source pixel, then the 1.5x area filter.
Rgb Reference(const std::vector<std::uint8_t>& raw, jc::Bayer pat, int x0,
              int y0, int ox, int oy) {
  const auto at_px = [&](int sx, int sy) {
    const auto at = [&](int dx, int dy) {
      return static_cast<int>(raw[static_cast<std::size_t>(
          jc::Reflect(sy + dy, kRawH) * kRawW + jc::Reflect(sx + dx, kRawW))]);
    };
    const jc::Bgr c = jc::Demosaic(pat, sx, sy, at);
    return Rgb{c.r, c.g, c.b};
  };
  static constexpr int kW[2][3] = {{2, 1, 0}, {0, 1, 2}};
  int r = 0, g = 0, b = 0;
  for (int j = 0; j < 3; ++j) {
    for (int i = 0; i < 3; ++i) {
      const int w = kW[oy % 2][j] * kW[ox % 2][i];
      const Rgb p = at_px(x0 + ox / 2 * 3 + i, y0 + oy / 2 * 3 + j);
      r += w * p.r;
      g += w * p.g;
      b += w * p.b;
    }
  }
  return {(r + 4) / 9, (g + 4) / 9, (b + 4) / 9};
}

// Average kernel time over `runs` launches, after a warm-up.
float TimeKernel(const jc::PreprocessParams& p, const jc::FrameBuffers& out,
                 cudaStream_t s, int runs) {
  for (int k = 0; k < 50; ++k) JC_CUDA_CHECK(jc::LaunchPreprocess(p, out, s));
  jc::cuda::Event a = jc::cuda::MakeEvent();
  jc::cuda::Event b = jc::cuda::MakeEvent();
  JC_CUDA_CHECK(cudaEventRecord(a.get(), s));
  for (int k = 0; k < runs; ++k) JC_CUDA_CHECK(jc::LaunchPreprocess(p, out, s));
  JC_CUDA_CHECK(cudaEventRecord(b.get(), s));
  JC_CUDA_CHECK(cudaEventSynchronize(b.get()));
  float ms = 0;
  JC_CUDA_CHECK(cudaEventElapsedTime(&ms, a.get(), b.get()));
  return ms / static_cast<float>(runs);
}

}  // namespace

int main() {
  std::vector<std::uint8_t> raw(static_cast<std::size_t>(kRawW * kRawH));
  std::mt19937 rng(7);
  for (auto& v : raw) v = static_cast<std::uint8_t>(rng() & 0xff);
  jc::cuda::Stream stream = jc::cuda::MakeStream();
  cudaStream_t s = stream.get();
  auto d_raw = jc::cuda::MakeDevice<std::uint8_t>(raw.size());
  JC_CUDA_CHECK(
      cudaMemcpy(d_raw.get(), raw.data(), raw.size(), cudaMemcpyHostToDevice));

  jc::FramePool pool(1, {kOutW, kOutH, true, true, true, true});
  auto frame = pool.Acquire();
  jc::PreprocessParams p{d_raw.get(), kRawW, kRawH, jc::kRggb, 0,    1,
                         kOutW,       kOutH, true,  false,     false};
  const std::size_t plane = static_cast<std::size_t>(kOutW) * kOutH;
  int bad = 0;
  double max_tensor_err = 0;
  // Both load strategies must match the CPU reference exactly.
  for (const bool tiled : {false, true}) {
    p.shared_tile = tiled;
    JC_CUDA_CHECK(jc::LaunchPreprocess(p, frame->buffers(), s));
    JC_CUDA_CHECK(cudaStreamSynchronize(s));
    const std::uint8_t* bgr = frame->buffers().bgr;
    std::vector<__half> tensor(static_cast<std::size_t>(3 * kOutW * kOutH));
    JC_CUDA_CHECK(cudaMemcpy(tensor.data(), frame->buffers().tensor,
                             tensor.size() * sizeof(__half),
                             cudaMemcpyDeviceToHost));
    // Every row near the edges (tile and image borders) plus every 7th row.
    for (int y = 0; y < kOutH; ++y) {
      if (y > 9 && y < kOutH - 10 && y % 7 != 0) continue;
      for (int x = 0; x < kOutW; ++x) {
        const Rgb want = Reference(raw, jc::kRggb, 0, 1, x, y);
        const std::size_t i = static_cast<std::size_t>(y) * kOutW + x;
        if (bgr[i * 3] != want.b || bgr[i * 3 + 1] != want.g ||
            bgr[i * 3 + 2] != want.r) {
          ++bad;
        }
        const double tb = __half2float(tensor[i]);
        const double tg = __half2float(tensor[plane + i]);
        const double tr = __half2float(tensor[2 * plane + i]);
        max_tensor_err =
            std::fmax(max_tensor_err, std::fabs(tb - want.b / 255.0));
        max_tensor_err =
            std::fmax(max_tensor_err, std::fabs(tg - want.g / 255.0));
        max_tensor_err =
            std::fmax(max_tensor_err, std::fabs(tr - want.r / 255.0));
      }
    }
  }
  p.shared_tile = false;
  // NV12 sanity: luma and chroma of a flat grey frame are exact.
  std::vector<std::uint8_t> grey(raw.size(), 128);
  JC_CUDA_CHECK(cudaMemcpy(d_raw.get(), grey.data(), grey.size(),
                           cudaMemcpyHostToDevice));
  JC_CUDA_CHECK(jc::LaunchPreprocess(p, frame->buffers(), s));
  JC_CUDA_CHECK(cudaStreamSynchronize(s));
  const std::uint8_t* nv12 = frame->buffers().nv12;
  const bool nv12_ok = nv12[0] == ((220 * 128 + 128) >> 8) + 16 &&
                       nv12[plane] == 128 && nv12[plane + 1] == 128;

  // Timing: input in device memory, and in registered (mapped) host memory
  // like a real SDK receive buffer.
  JC_CUDA_CHECK(
      cudaHostRegister(raw.data(), raw.size(),
                       cudaHostRegisterMapped | cudaHostRegisterReadOnly));
  void* mapped = nullptr;
  JC_CUDA_CHECK(cudaHostGetDevicePointer(&mapped, raw.data(), 0));
  for (const bool fp16 : {true, false}) {
    for (const bool tiled : {false, true}) {
      jc::FramePool tp(1, {kOutW, kOutH, true, fp16, false, true});
      auto f = tp.Acquire();
      jc::PreprocessParams q = p;
      q.tensor_fp16 = fp16;
      q.shared_tile = tiled;
      const float device_ms = TimeKernel(q, f->buffers(), s, 1000);
      q.raw = static_cast<const std::uint8_t*>(mapped);
      const float mapped_ms = TimeKernel(q, f->buffers(), s, 1000);
      std::printf(
          "  kernel tensor %s + nv12, %-12s: %.4f ms device memory, %.4f ms "
          "mapped host memory\n",
          fp16 ? "fp16" : "fp32", tiled ? "shared tile" : "direct ldg",
          device_ms, mapped_ms);
    }
  }
  JC_CUDA_CHECK(cudaHostUnregister(raw.data()));

  std::printf(
      "  bgr mismatches vs CPU reference: %d, max tensor error %.5f, nv12 "
      "grey ok: %d\n",
      bad, max_tensor_err, nv12_ok);
  const bool ok = bad == 0 && max_tensor_err < 1.0 / 512 && nv12_ok;
  frame.reset();
  std::printf(ok ? "preprocess_test passed\n" : "preprocess_test FAILED\n");
  return ok ? 0 : 1;
}
