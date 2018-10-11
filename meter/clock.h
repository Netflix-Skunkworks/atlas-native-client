#pragma once

#include <cstdint>
#include <memory>

namespace atlas {
namespace meter {
class Clock {
 public:
  virtual int64_t WallTime() const noexcept = 0;

  virtual int64_t MonotonicTime() const noexcept = 0;

  virtual ~Clock() = default;
};

class SystemClock : public Clock {
 public:
  int64_t WallTime() const noexcept override;

  int64_t MonotonicTime() const noexcept override;
};

class WrappedClock : public Clock {
 public:
  explicit WrappedClock(const Clock* clock) : underlying_(clock) {}
  void SetOffset(int offset) { offset_ = offset; }
  int64_t WallTime() const noexcept override {
    return underlying_->WallTime() + offset_;
  }
  int64_t MonotonicTime() const noexcept override {
    return underlying_->MonotonicTime() + offset_ * 1000000;
  }

 private:
  const Clock* underlying_;
  int offset_ = 0;
};

}  // namespace meter
}  // namespace atlas
