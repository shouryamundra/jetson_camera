// Verifies the demosaic math for all four Bayer layouts without a GPU: guards
// the channel order and parity handling that the CUDA kernel reuses.

#include "src/bayer.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

namespace jc = jetson_camera;

constexpr int kWidth = 64;
constexpr int kHeight = 48;

struct Rgb {
  int r, g, b;
};

// Colour of the synthetic scene at (x, y): a constant or a linear ramp.
using Scene = Rgb (*)(int, int);

Rgb Flat(int, int) { return {200, 100, 50}; }
Rgb Ramp(int x, int y) { return {x * 2, 255 - y * 4, (x + y) * 2}; }

std::size_t At(int x, int y) {
  return static_cast<std::size_t>(y * kWidth + x);
}

// Keeps only the colour sample the sensor would record at each site.
std::vector<std::uint8_t> Mosaic(const jc::Bayer& p, Scene scene) {
  std::vector<std::uint8_t> raw(static_cast<std::size_t>(kWidth * kHeight));
  for (int y = 0; y < kHeight; ++y) {
    for (int x = 0; x < kWidth; ++x) {
      const bool red_row = ((y ^ p.red_row) & 1) == 0;
      const bool red_col = ((x ^ p.red_col) & 1) == 0;
      const Rgb c = scene(x, y);
      const int sample = red_row == red_col ? (red_row ? c.r : c.b) : c.g;
      raw[At(x, y)] = static_cast<std::uint8_t>(sample);
    }
  }
  return raw;
}

// Largest per-channel error over the pixels in [margin, size - margin).
int MaxError(const jc::Bayer& p, Scene scene, int margin) {
  const auto raw = Mosaic(p, scene);
  int worst = 0;
  for (int y = margin; y < kHeight - margin; ++y) {
    for (int x = margin; x < kWidth - margin; ++x) {
      const auto sample = [&](int dx, int dy) {
        return static_cast<int>(
            raw[At(jc::Reflect(x + dx, kWidth), jc::Reflect(y + dy, kHeight))]);
      };
      const jc::Bgr got = jc::Demosaic(p, x, y, sample);
      const Rgb want = scene(x, y);
      worst = std::max({worst, std::abs(got.r - want.r),
                        std::abs(got.g - want.g), std::abs(got.b - want.b)});
    }
  }
  return worst;
}

}  // namespace

int main() {
  const struct {
    const char* name;
    jc::Bayer pattern;
  } layouts[] = {{"RGGB", jc::kRggb},
                 {"GRBG", jc::kGrbg},
                 {"GBRG", jc::kGbrg},
                 {"BGGR", jc::kBggr}};

  int failures = 0;
  for (const auto& l : layouts) {
    // A flat colour must come back exactly everywhere, borders included: a
    // swapped channel or a wrong parity anywhere shows up here.
    const int flat_error = MaxError(l.pattern, Flat, 0);
    // A linear ramp is reproduced to rounding away from the border (the
    // reflection bends it there).
    const int ramp_error = MaxError(l.pattern, Ramp, 1);
    const bool pass = flat_error == 0 && ramp_error <= 1;
    std::printf("%s: flat error %d, ramp error %d  %s\n", l.name, flat_error,
                ramp_error, pass ? "ok" : "FAIL");
    failures += !pass;
  }

  // Reflect-101 keeps parity: the neighbour across the border has the same
  // parity as the pixel.
  const bool reflect_ok = jc::Reflect(-1, 10) == 1 &&
                          jc::Reflect(10, 10) == 8 && jc::Reflect(0, 10) == 0 &&
                          jc::Reflect(9, 10) == 9;
  std::printf("reflect: %s\n", reflect_ok ? "ok" : "FAIL");
  failures += !reflect_ok;
  return failures ? 1 : 0;
}
