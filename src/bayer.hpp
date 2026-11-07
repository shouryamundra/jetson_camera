// Purpose: Bayer-pattern layout and bilinear demosaic of a single pixel.
// Paradigm: Header-only pure functions shared by host tests and the CUDA kernel.
// Role: Single source of truth for the demosaic math.

#ifndef IO__BAYER_HPP
#define IO__BAYER_HPP

#include <cstdint>

#ifdef __CUDACC__
#define IO_HD __host__ __device__ __forceinline__
#else
#define IO_HD inline
#endif

namespace io
{
// Parity of the row and column that hold the red sample inside every 2x2 cell.
//   RGGB: {0, 0}   GRBG: {0, 1}   GBRG: {1, 0}   BGGR: {1, 1}
struct Bayer
{
  int red_row;
  int red_col;
};

struct Bgr
{
  uint8_t b, g, r;
};

// Reflect-101 (-1 -> 1, n -> n - 2) keeps the Bayer parity of border neighbours.
IO_HD int reflect(int i, int n)
{
  if (i < 0) i = -i;
  if (i >= n) i = 2 * (n - 1) - i;
  return i < 0 ? 0 : (i >= n ? n - 1 : i);  // only reachable for n < 2 or far out-of-range input
}

// Bilinear reconstruction of the pixel at absolute sensor coordinates (x, y).
// at(dx, dy) returns the raw sample at offset (dx, dy), each in [-1, 1].
template <typename Sample>
IO_HD Bgr demosaic(const Bayer & pattern, int x, int y, Sample at)
{
  const bool red_row = ((y ^ pattern.red_row) & 1) == 0;
  const bool red_col = ((x ^ pattern.red_col) & 1) == 0;
  const int c = at(0, 0);

  if (red_row == red_col) {  // red or blue site: the other two channels are interpolated
    const int g = (at(-1, 0) + at(1, 0) + at(0, -1) + at(0, 1) + 2) >> 2;
    const int d = (at(-1, -1) + at(1, -1) + at(-1, 1) + at(1, 1) + 2) >> 2;
    return red_row ? Bgr{uint8_t(d), uint8_t(g), uint8_t(c)}
                   : Bgr{uint8_t(c), uint8_t(g), uint8_t(d)};
  }

  // green site: on a red row the horizontal neighbours are red and the vertical ones blue
  const int h = (at(-1, 0) + at(1, 0) + 1) >> 1;
  const int v = (at(0, -1) + at(0, 1) + 1) >> 1;
  return red_row ? Bgr{uint8_t(v), uint8_t(c), uint8_t(h)}
                 : Bgr{uint8_t(h), uint8_t(c), uint8_t(v)};
}

}  // namespace io

#endif  // IO__BAYER_HPP
