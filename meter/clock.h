#pragma once

#include <cstdint>
#include <memory>

namespace atlas {
namespace meter {
class Clock {
 public:
  virtual int64_t WallTime() const noexcept = 0;

  virtual int64_t MonotonicTime() const noexcept = 0;
};

class SystemClock : public Clock {
 public:
  int64_t WallTime() const noexcept override;

  int64_t MonotonicTime() const noexcept override;
};

class SystemClockWithOffset : public Clock {
 public:
  SystemClockWithOffset() noexcept : offset_{0} {}

  int64_t WallTime() const noexcept override {
    return clock.WallTime() + offset_;
  }

  int64_t MonotonicTime() const noexcept override {
    return clock.MonotonicTime();
  }

  void SetOffset(int64_t offset) noexcept { offset_ = offset; }

 private:
  SystemClock clock;
  int64_t offset_;
};

}  // namespace meter
}  // namespace atlas
