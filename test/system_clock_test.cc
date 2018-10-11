#include "../meter/clock.h"
#include <gtest/gtest.h>

using atlas::meter::SystemClock;
using atlas::meter::WrappedClock;

TEST(SystemClock, WallTime) {
  SystemClock clock;

  auto clock_now = clock.WallTime();
  auto secs = time(nullptr);

  auto clock_secs = clock_now / 1000;
  EXPECT_LE(std::abs(secs - clock_secs), 1);
}

TEST(WrappedClock, WallTimeOffset) {
  SystemClock sys_clock;
  WrappedClock clock{&sys_clock};

  clock.SetOffset(5000);
  auto clock_now = clock.WallTime();
  auto secs = time(nullptr);

  auto clock_secs = clock_now / 1000;
  auto delta = std::abs(secs - clock_secs);
  EXPECT_TRUE(delta >= 5 && delta <= 6);
}

TEST(SystemClock, MonotonicTime) {
  SystemClock clock;
  for (auto j = 0; j < 10; ++j) {
    auto prev = clock.MonotonicTime();
    for (auto i = 0; i < 100; ++i) {
      auto now = clock.MonotonicTime();
      EXPECT_GE(now - prev, 0);
    }
  }
}