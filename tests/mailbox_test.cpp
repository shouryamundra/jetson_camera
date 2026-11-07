// Purpose: Verify FrameMailbox ordering, freshness, tear-freedom and shutdown under contention.
// Paradigm: Self-contained assertion test with one producer thread and one slow consumer.
// Role: Guards the capture-to-consumer handoff that the Jetson pipeline depends on.

#include <chrono>
#include <cstdio>
#include <thread>

#include "frame_mailbox.hpp"

namespace
{
struct Frame
{
  uint64_t value[8];  // every element equals the sequence number unless the slot is torn
};

constexpr uint64_t kFrames = 300000;
}  // namespace

int main()
{
  io::FrameMailbox<Frame> mailbox;

  std::thread producer([&] {
    for (uint64_t seq = 1; seq <= kFrames; ++seq) {
      Frame & slot = mailbox.write_slot();
      for (auto & v : slot.value) v = seq;
      mailbox.publish();
    }
  });

  uint64_t last = 0, read = 0;
  int errors = 0;
  std::thread consumer([&] {
    while (const Frame * frame = mailbox.acquire()) {
      const uint64_t seq = frame->value[0];
      for (const auto v : frame->value) errors += v != seq;  // torn slot
      errors += seq <= last;                                  // went backwards or repeated
      last = seq;
      ++read;
      if (read % 64 == 0) std::this_thread::sleep_for(std::chrono::microseconds(200));  // slow
    }
  });

  producer.join();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));  // consumer drains the last frame
  mailbox.close();
  consumer.join();

  const bool saw_last = last == kFrames;  // freshness: the final frame is never lost
  const bool dropped = read < kFrames;    // the slow consumer must have skipped stale frames
  std::printf(
    "published %llu, read %llu, errors %d, last frame %s, stale frames skipped %s\n",
    (unsigned long long)kFrames, (unsigned long long)read, errors, saw_last ? "seen" : "LOST",
    dropped ? "yes" : "NO");

  // reset() makes the mailbox reusable after close().
  mailbox.reset();
  Frame & slot = mailbox.write_slot();
  for (auto & v : slot.value) v = 7;
  mailbox.publish();
  const Frame * again = mailbox.acquire();
  const bool reuse_ok = again && again->value[0] == 7;
  std::printf("reuse after reset: %s\n", reuse_ok ? "ok" : "FAIL");

  return (errors == 0 && saw_last && dropped && reuse_ok) ? 0 : 1;
}
