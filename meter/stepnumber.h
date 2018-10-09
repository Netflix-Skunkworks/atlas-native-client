#pragma once

#include <atomic>
#include <cstdint>
#include "clock.h"

namespace atlas {
namespace meter {

template <typename T>
class StepNumber {
  template <typename CT>
  struct type {};

 public:
  StepNumber(const StepNumber& s) noexcept
      : init_(s.init_),
        step_millis_(s.step_millis_),
        clock_(s.clock_),
        previous_(s.previous_.load(std::memory_order_relaxed)),
        current_(s.current_.load(std::memory_order_relaxed)),
        last_init_pos_(s.last_init_pos_.load(std::memory_order_relaxed)) {}

  StepNumber& operator=(const StepNumber& s) noexcept {
    init_ = s.init_;
    step_millis_ = s.step_millis_;
    clock_ = s.clock_;
    previous_ = s.previous_.load(std::memory_order_relaxed);
    current_ = s.current_.load(std::memory_order_relaxed);
    last_init_pos_ = s.last_init_pos_.load(std::memory_order_relaxed);
    return *this;
  }

  StepNumber(T init, int64_t step_millis, const Clock* clock) noexcept
      : init_(init),
        step_millis_(step_millis),
        clock_(clock),
        previous_(init),
        current_(init),
        last_init_pos_(clock->WallTime() / step_millis) {}

  /// Get the value for the last completed interval
  T Poll() const noexcept {
    RollCountNow();
    return previous_.load(std::memory_order_relaxed);
  }

  /// Get the value for the current interval
  T Current() const noexcept {
    RollCountNow();
    return current_.load(std::memory_order_relaxed);
  }

  /// Add amount to the current value
  void Add(T amount) noexcept {
    RollCountNow();
    AddAmount(amount, type<T>());
  }

  /// Atomically update the current bucket to be max(current, value)
  void UpdateCurrentMax(T value) noexcept {
    auto m = Current();
    while (value > m) {
      if (current_.compare_exchange_weak(m, value)) {
        break;
      }
      m = current_.load(std::memory_order_relaxed);
    }
  }

  int64_t StepMillis() const noexcept { return step_millis_; }

 private:
  T init_;
  int64_t step_millis_;
  const Clock* clock_;
  mutable std::atomic<T> previous_;
  mutable std::atomic<T> current_;
  mutable std::atomic<T> last_init_pos_;

  void RollCount(int64_t now) const noexcept {
    const auto step_time = now / step_millis_;
    auto last_init = last_init_pos_.load();
    if (last_init < step_time &&
        last_init_pos_.compare_exchange_strong(last_init, step_time)) {
      const auto v = current_.exchange(init_);
      // Need to check if there was any activity during the previous step
      // interval. If there was then the init position will move forward by 1,
      // otherwise it will be older. No activity
      // means the previous interval should be set to the `init` value.
      previous_.store((last_init == step_time - 1) ? v : init_);
    }
  }
  void RollCountNow() const noexcept { RollCount(clock_->WallTime()); }

  template <typename CT>
  void AddAmount(T amount, type<CT>) noexcept {
    current_ += amount;
  }

  void AddAmount(double amount, type<double>) noexcept {
    double current;
    do {
      current = current_.load(std::memory_order_relaxed);
    } while (!current_.compare_exchange_weak(current, current + amount));
  }
};

using StepLong = StepNumber<int64_t>;
}  // namespace meter
}  // namespace atlas
