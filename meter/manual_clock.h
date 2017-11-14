#pragma once

#include <atomic>
#include "clock.h"

namespace atlas {
namespace meter {
class ManualClock : public Clock {
 public:
  ManualClock(int64_t wall = 0, int64_t monotonic = 0) noexcept
      : wall_(wall), monotonic_(monotonic) {}

  virtual int64_t WallTime() const noexcept override { return wall_.load(); }

  virtual int64_t MonotonicTime() const noexcept override {
    return monotonic_.load();
  }

  void SetWall(int64_t n) const noexcept { wall_.store(n); }

  void SetMonotonic(int64_t n) const noexcept { monotonic_.store(n); }

 private:
  // the following are marked mutable to help with testing
  mutable std::atomic<int64_t> wall_;
  mutable std::atomic<int64_t> monotonic_;
};
}  // namespace meter
}  // namespace atlas
