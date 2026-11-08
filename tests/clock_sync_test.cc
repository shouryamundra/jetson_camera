// Checks that ClockSync recovers the host time of camera stamps under jitter,
// drift, outliers and clock restarts. Host-only.

#include "src/clock_sync.h"

#include <cstdio>
#include <cstdlib>
#include <random>

namespace {

int failures = 0;

void ExpectNear(const char* what, std::int64_t got, std::int64_t want,
                std::int64_t tol) {
  if (std::llabs(got - want) > tol) {
    std::printf("FAIL %s: got %lld want %lld (+-%lld)\n", what,
                static_cast<long long>(got),   // NOLINT(runtime/int)
                static_cast<long long>(want),  // NOLINT(runtime/int)
                static_cast<long long>(tol));  // NOLINT(runtime/int)
    ++failures;
  }
}

}  // namespace

int main() {
  using jetson_camera::ClockSync;
  std::mt19937 rng(1);
  std::exponential_distribution<double> jitter(1.0 / 40.0);  // ~40 us, >= 0.
  const std::int64_t offset = 1'700'000'000'000'000;  // Host - camera epoch.
  const std::int64_t latency = 4700;
  const auto noise = [&] { return static_cast<std::int64_t>(jitter(rng)); };

  {  // Steady stream with jitter: error within a few us after a few frames.
    ClockSync sync;
    for (int k = 0; k < 2000; ++k) {
      const std::int64_t stamp = 1'000'000 + k * 4600;
      const std::int64_t host =
          sync.ToHost(stamp, stamp + offset + latency + noise(), latency);
      if (k > 50) ExpectNear("jitter", host, stamp + offset, 15);
    }
  }
  {  // 50 ppm drift: the 2 s window follows it (error <= 100 us + jitter).
    ClockSync sync;
    for (int k = 0; k < 10000; ++k) {
      const std::int64_t stamp = k * 4600;
      const auto drifted =
          static_cast<std::int64_t>(static_cast<double>(stamp) * (1 + 50e-6));
      const std::int64_t host =
          sync.ToHost(stamp, drifted + offset + latency + noise(), latency);
      if (k > 50) ExpectNear("drift", host, drifted + offset, 120);
    }
  }
  {  // One badly delayed frame (300 ms) must not move the estimate.
    ClockSync sync;
    std::int64_t stamp = 0;
    for (int k = 0; k < 100; ++k, stamp += 4600) {
      sync.ToHost(stamp, stamp + offset + latency, latency);
    }
    ExpectNear("outlier",
               sync.ToHost(stamp, stamp + offset + latency + 300'000, latency),
               stamp + offset, 1);
    stamp += 4600;
    ExpectNear("after outlier",
               sync.ToHost(stamp, stamp + offset + latency, latency),
               stamp + offset, 1);
  }
  {  // Camera clock restarts at 0 (reconnect): re-locks after 5 frames.
    ClockSync sync;
    std::int64_t stamp = 50'000'000;
    for (int k = 0; k < 100; ++k, stamp += 4600) {
      sync.ToHost(stamp, stamp + offset + latency, latency);
    }
    const std::int64_t host_now = stamp + offset;
    std::int64_t host = 0;
    for (int k = 0; k < 10; ++k) {
      host = sync.ToHost(k * 4600, host_now + k * 4600 + latency, latency);
    }
    ExpectNear("restart", host, host_now + 9 * 4600, 1);
  }
  {  // Reset() forgets history immediately.
    ClockSync sync;
    sync.ToHost(0, offset + latency, latency);
    sync.Reset();
    ExpectNear("reset", sync.ToHost(0, 7 + latency, latency), 7, 0);
  }
  std::printf(failures ? "clock_sync_test FAILED\n"
                       : "clock_sync_test passed\n");
  return failures ? 1 : 0;
}
