// Purpose: Bayer-pattern layout and bilinear demosaic of a single pixel.
// Paradigm: Header-only pure functions shared by host tests and the CUDA kernel.
// Role: Single source of truth for the demosaic math.

#ifndef IO__BAYER_HPP
#define IO__BAYER_HPP

#include <cstdint>

#ifdef __CUDACC__
#define IO_HD __host__ __device__ constexpr
#else
#define IO_HD constexpr
#endif

namespace io
{
// Parity of the row and column that hold the red sample inside every 2x2 cell.
struct Bayer
{
  int red_row;
  int red_col;
};

inline constexpr Bayer kRggb{0, 0};
inline constexpr Bayer kGrbg{0, 1};
inline constexpr Bayer kGbrg{1, 0};
inline constexpr Bayer kBggr{1, 1};

struct Bgr
{
  std::uint8_t b, g, r;
};

// Reflect-101 (-1 -> 1, n -> n - 2) keeps the Bayer parity of border neighbours.
IO_HD int reflect(int i, const int n)
{
  if (i < 0) i = -i;
  if (i >= n) i = 2 * (n - 1) - i;
  return i < 0 ? 0 : (i >= n ? n - 1 : i);  // only reachable for n < 2 or far out-of-range input
}

// Bilinear reconstruction of the pixel at absolute sensor coordinates (x, y).
// at(dx, dy) returns the raw sample at offset (dx, dy), each in [-1, 1].
template <typename Sample>
IO_HD Bgr demosaic(const Bayer & pattern, const int x, const int y, const Sample & at)
{
  const auto byte = [](const int v) { return static_cast<std::uint8_t>(v); };
  const bool red_row = ((y ^ pattern.red_row) & 1) == 0;
  const bool red_col = ((x ^ pattern.red_col) & 1) == 0;
  const int c = at(0, 0);

  if (red_row == red_col) {  // red or blue site: the other two channels are interpolated
    const int g = (at(-1, 0) + at(1, 0) + at(0, -1) + at(0, 1) + 2) >> 2;
    const int d = (at(-1, -1) + at(1, -1) + at(-1, 1) + at(1, 1) + 2) >> 2;
    return red_row ? Bgr{byte(d), byte(g), byte(c)} : Bgr{byte(c), byte(g), byte(d)};
  }

  // green site: on a red row the horizontal neighbours are red and the vertical ones blue
  const int h = (at(-1, 0) + at(1, 0) + 1) >> 1;
  const int v = (at(0, -1) + at(0, 1) + 1) >> 1;
  return red_row ? Bgr{byte(v), byte(c), byte(h)} : Bgr{byte(h), byte(c), byte(v)};
}

}  // namespace io

#endif  // IO__BAYER_HPP
