// Purpose: Verify the demosaic math for all four Bayer layouts without a GPU.
// Paradigm: Self-contained assertion test over synthetic mosaics.
// Role: Guards channel order and parity handling that the CUDA kernel reuses.

#include <cstdio>
#include <cstdlib>
#include <vector>

#include "bayer.hpp"

namespace
{
constexpr int kWidth = 64;
constexpr int kHeight = 48;

struct Rgb
{
  int r, g, b;
};

// Colour of the synthetic scene at (x, y); a constant or a linear ramp.
using Scene = Rgb (*)(int, int);

Rgb flat(int, int) { return {200, 100, 50}; }
Rgb ramp(int x, int y) { return {x * 2, 255 - y * 4, (x + y) * 2}; }

// Keep only the colour sample the sensor would record at each site.
std::vector<uint8_t> mosaic(const io::Bayer & p, Scene scene)
{
  std::vector<uint8_t> raw(kWidth * kHeight);
  for (int y = 0; y < kHeight; ++y)
    for (int x = 0; x < kWidth; ++x) {
      const bool red_row = ((y ^ p.red_row) & 1) == 0;
      const bool red_col = ((x ^ p.red_col) & 1) == 0;
      const Rgb c = scene(x, y);
      raw[y * kWidth + x] = red_row == red_col ? (red_row ? c.r : c.b) : c.g;
    }
  return raw;
}

// Largest per-channel error over the pixels in [margin, size - margin).
int max_error(const io::Bayer & p, Scene scene, int margin)
{
  const auto raw = mosaic(p, scene);
  int worst = 0;
  for (int y = margin; y < kHeight - margin; ++y)
    for (int x = margin; x < kWidth - margin; ++x) {
      const auto at = [&](int dx, int dy) {
        return raw[io::reflect(y + dy, kHeight) * kWidth + io::reflect(x + dx, kWidth)];
      };
      const io::Bgr got = io::demosaic(p, x, y, at);
      const Rgb want = scene(x, y);
      for (int e : {std::abs(got.r - want.r), std::abs(got.g - want.g), std::abs(got.b - want.b)})
        if (e > worst) worst = e;
    }
  return worst;
}

}  // namespace

int main()
{
  const struct
  {
    const char * name;
    io::Bayer pattern;
  } layouts[] = {{"RGGB", {0, 0}}, {"GRBG", {0, 1}}, {"GBRG", {1, 0}}, {"BGGR", {1, 1}}};

  int failures = 0;
  for (const auto & l : layouts) {
    // A flat colour must come back exactly everywhere, borders included. A swapped channel or
    // a wrong parity anywhere shows up here.
    const int flat_error = max_error(l.pattern, flat, 0);
    // A linear ramp is reproduced to rounding away from the border (reflection bends it there).
    const int ramp_error = max_error(l.pattern, ramp, 1);
    const bool pass = flat_error == 0 && ramp_error <= 1;
    std::printf("%s: flat error %d, ramp error %d  %s\n", l.name, flat_error, ramp_error,
                pass ? "ok" : "FAIL");
    failures += !pass;
  }

  // Reflect-101 keeps parity: the neighbour across the border has the same parity as the pixel.
  const bool reflect_ok = io::reflect(-1, 10) == 1 && io::reflect(10, 10) == 8 &&
                          io::reflect(0, 10) == 0 && io::reflect(9, 10) == 9;
  std::printf("reflect: %s\n", reflect_ok ? "ok" : "FAIL");
  failures += !reflect_ok;

  return failures ? 1 : 0;
}
