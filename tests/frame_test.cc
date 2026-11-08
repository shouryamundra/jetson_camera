// Checks FramePool lifetimes and LatestFrame semantics, including under
// concurrency. Needs the CUDA runtime (pool memory) but no camera.

#include "src/frame.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <utility>
#include <vector>

namespace {

namespace jc = jetson_camera;

int failures = 0;

#define EXPECT(c)                                      \
  do {                                                 \
    if (!(c)) {                                        \
      std::printf("FAIL line %d: %s\n", __LINE__, #c); \
      ++failures;                                      \
    }                                                  \
  } while (0)

}  // namespace

int main() {
  using std::chrono::milliseconds;
  const jc::FramePool::Layout layout{64, 32, true, true, true, true};
  {
    jc::FramePool pool(3, layout);
    EXPECT(pool.free_count() == 3);
    auto a = pool.Acquire();
    auto b = pool.Acquire();
    auto c = pool.Acquire();
    EXPECT(a && b && c);
    EXPECT(!pool.Acquire());  // Exhausted: nothing, never blocks.
    EXPECT(a->buffers().tensor && a->buffers().bgr && a->buffers().nv12);
    EXPECT(a->buffers().bgr != b->buffers().bgr);
    {
      jc::FrameRef copy = *a;  // Second reference.
      a.reset();
      EXPECT(pool.free_count() == 0);  // Still held by the copy.
    }
    EXPECT(pool.free_count() == 1);
    b.reset();
    c.reset();
    EXPECT(pool.free_count() == 3);
  }
  {  // Newest wins, the old frame is released on replace, wait times out,
     // Close() wakes waiters.
    jc::FramePool pool(4, layout);
    jc::LatestFrame box;
    EXPECT(!box.WaitNewer(0, milliseconds(1)));
    for (std::uint64_t s = 1; s <= 3; ++s) {
      auto f = pool.Acquire();
      pool.MutableInfo(*f).seq = s;
      box.Publish(std::move(*f));
    }
    EXPECT(pool.free_count() == 3);  // Only the newest is held.
    auto got = box.WaitNewer(0, milliseconds(1));
    EXPECT(got && got->info().seq == 3);
    EXPECT(!box.WaitNewer(3, milliseconds(1)));  // Nothing newer.
    std::thread closer([&] {
      std::this_thread::sleep_for(milliseconds(20));
      box.Close();
    });
    const auto t0 = std::chrono::steady_clock::now();
    EXPECT(!box.WaitNewer(3, std::chrono::seconds(5)));
    EXPECT(std::chrono::steady_clock::now() - t0 < std::chrono::seconds(1));
    closer.join();
    got.reset();
    EXPECT(pool.free_count() == 4);
  }
  {  // Stress: producer at full speed, two consumers holding frames; no leak,
     // seq monotonic per consumer.
    jc::FramePool pool(6, layout);
    jc::LatestFrame box;
    std::atomic<bool> done{false};
    std::atomic<int> drops{0};
    std::atomic<bool> order_ok{true};
    std::thread producer([&] {
      for (std::uint64_t s = 1; s <= 200000; ++s) {
        auto f = pool.Acquire();
        if (!f) {
          ++drops;
          continue;
        }
        pool.MutableInfo(*f).seq = s;
        box.Publish(std::move(*f));
      }
      done = true;
    });
    std::vector<std::thread> consumers;
    for (int k = 0; k < 2; ++k) {
      consumers.emplace_back([&] {
        std::uint64_t last = 0;
        while (!done) {
          auto f = box.WaitNewer(last, milliseconds(10));
          if (!f) continue;
          if (f->info().seq <= last) order_ok = false;
          last = f->info().seq;
          std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
      });
    }
    producer.join();
    for (auto& t : consumers) t.join();
    box.Close();
    EXPECT(order_ok);
    EXPECT(pool.free_count() == 6);
    std::printf("  stress: %d producer drops of 200000\n", drops.load());
  }
  std::printf(failures ? "frame_test FAILED\n" : "frame_test passed\n");
  return failures ? 1 : 0;
}
