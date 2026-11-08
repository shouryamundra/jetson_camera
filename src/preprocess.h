// The fused Bayer -> {model tensor, BGR image, NV12 video frame} GPU pass: one
// kernel launch per frame, every output pixel computed once from raw data.

#ifndef JETSON_CAMERA_SRC_PREPROCESS_H_
#define JETSON_CAMERA_SRC_PREPROCESS_H_

#include <cuda_runtime.h>

#include <cstdint>

#include "src/bayer.h"
#include "src/frame.h"

namespace jetson_camera {

// Source window: a (3*out_width/2) x (3*out_height/2) rectangle of the raw
// image at (x0, y0). It is demosaiced (bilinear) and area-downscaled by
// exactly 1.5 (each 2x2 output block covers 3x3 source pixels), so out_width
// and out_height must be even.
struct PreprocessParams {
  const std::uint8_t* raw;  // GPU-readable Bayer image.
  int raw_width;            // Also the row pitch in bytes.
  int raw_height;
  Bayer pattern;  // For the raw image's (0, 0) pixel.
  int x0;
  int y0;
  int out_width;
  int out_height;
  bool tensor_fp16;
  bool tensor_rgb;  // Plane order; false = BGR (Project Otto's current order).
  // Load strategy for the raw window: false = per-thread read-only-cache
  // loads, true = shared-memory tile per block. See preprocess_test timings.
  bool shared_tile = false;
};

// Queues the kernel on `stream`. Returns the launch error (never throws), so
// it is safe on the capture hot path.
[[nodiscard]] cudaError_t LaunchPreprocess(const PreprocessParams& p,
                                           const FrameBuffers& out,
                                           cudaStream_t stream) noexcept;

}  // namespace jetson_camera

#endif  // JETSON_CAMERA_SRC_PREPROCESS_H_
