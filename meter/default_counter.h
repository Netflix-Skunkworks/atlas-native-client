#include <array>
#include <utility>
#include "counter.h"
#include "stepnumber.h"
#include "statistic.h"

#pragma once

namespace atlas {
namespace meter {

template <typename T>
class DefaultCounterNumber : public Meter, public CounterNumber<T> {
  template <typename CT>
  struct type {};

 public:
  DefaultCounterNumber(IdPtr id, const Clock& clock, int64_t freq_millis)
      : Meter(WithDefaultTagForId(id, statistic::count), clock),
        step_number_(0, freq_millis, clock),
        value_(0) {
    Updated();
  }

  std::ostream& Dump(std::ostream& os) const override {
    os << "DefaultCounter(id=" << *id_ << ",c=" << step_number_.Current()
       << ")";
    return os;
  }

  Measurements Measure() const override {
    auto stepMillis = step_number_.StepMillis();
    auto poller_freq_secs = stepMillis / 1000.0;
    auto rate = step_number_.Poll() / poller_freq_secs;
    auto now = clock_.WallTime();
    auto offset = now % stepMillis;
    auto start_step = now - offset;
    return Measurements{Measurement{id_, start_step, rate}};
  }

  void Increment() noexcept override { Add(1); }

  void Add(T amount) noexcept override {
    step_number_.Add(amount);
    Add(amount, type<T>());
    Updated();
  }

  T Count() const noexcept override {
    return value_.load(std::memory_order_relaxed);
  }

  const char* GetClass() const noexcept override { return "DefaultCounter"; }

 private:
  mutable StepNumber<T> step_number_;
  std::atomic<T> value_;  // to keep the total count

  template <typename CT>
  void Add(T amount, type<CT>) noexcept {
    value_ += amount;
  }

  void Add(double amount, type<double>) noexcept {
    T current;
    do {
      current = value_.load(std::memory_order_relaxed);
    } while (!value_.compare_exchange_weak(current, current + amount));
  }
};

using DefaultCounter = DefaultCounterNumber<int64_t>;
using DefaultDoubleCounter = DefaultCounterNumber<double>;
}  // namespace meter
}  // namespace atlas
