#pragma once

#include <array>
#include <utility>
#include "gauge.h"
#include "stepnumber.h"
#include "statistic.h"

namespace atlas {
namespace meter {

template <typename T>
class DefaultMaxGauge : public Meter, public Gauge<T> {
  static constexpr auto kMinValue = std::numeric_limits<T>::lowest();

 public:
  DefaultMaxGauge(IdPtr id, const Clock& clock, int64_t freq_millis)
      : Meter(WithDefaultGaugeTags(std::move(id), statistic::max), clock),
        value_(0),
        step_number_(kMinValue, freq_millis, &clock) {
    Updated();
  }

  void Update(T v) noexcept override {
    step_number_.UpdateCurrentMax(v);
    auto current = value_.load(std::memory_order_relaxed);
    value_.store(std::max(current, v), std::memory_order_relaxed);
    Updated();
  }

  double Value() const noexcept override {
    return value_.load(std::memory_order_relaxed);
  }

  std::ostream& Dump(std::ostream& os) const override {
    auto v = value_.load(std::memory_order_relaxed);
    os << "MaxGauge{" << *id_ << ", value=" << v << "}";
    return os;
  }

  Measurements Measure() const override {
    const auto maybe_max = step_number_.Poll();
    const double max = maybe_max != kMinValue ? maybe_max : myNaN;
    auto now = clock_.WallTime();
    auto stepMillis = step_number_.StepMillis();
    auto offset = now % stepMillis;
    auto start_step = now - offset;
    return Measurements{Measurement{id_, start_step, max}};
  }

  const char* GetClass() const noexcept override { return "DefaultMaxGauge"; }

 private:
  std::atomic<T> value_;  // to keep the local max
  mutable StepNumber<T> step_number_;
};

using DefaultMaxGaugeInt = DefaultMaxGauge<int64_t>;

}  // namespace meter
}  // namespace atlas
