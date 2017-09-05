#pragma once

#include <array>
#include "gauge.h"
#include "stepnumber.h"
#include "statistic.h"

namespace atlas {
namespace meter {

template <typename T>
class SubscriptionMaxGauge : public Meter, public Gauge<T> {
  using StepNumbers = std::array<StepNumber<T>*, MAX_POLLER_FREQ>;
  static constexpr auto kMinValue = std::numeric_limits<T>::lowest();

 public:
  SubscriptionMaxGauge(IdPtr id, const Clock& clock, Pollers& poller_frequency)
      : Meter(WithDefaultGaugeTags(id, statistic::max), clock),
        current_size_(0),
        value_(0),
        poller_frequency_(poller_frequency) {
    Updated();
    UpdatePollers();
  }

  void Update(T v) noexcept override {
    for (auto i = 0u; i < current_size_; ++i) {
      step_numbers.at(i)->UpdateCurrentMax(v);
    }
    auto current = value_.load(std::memory_order_relaxed);
    value_.store(std::max(current, v), std::memory_order_relaxed);
    Updated();
  }

  double Value() const noexcept override {
    return value_.load(std::memory_order_relaxed);
  }

  std::ostream& Dump(std::ostream& os) const override {
    auto v = value_.load(std::memory_order_relaxed);
    os << "MaxGauge{" << id_ << ", value=" << v << "}";
    return os;
  }

  Measurements MeasuresForPoller(size_t poller_idx) const override {
    if (current_size_ <= poller_idx) {
      return Measurements();
    }

    const auto maybe_max = step_numbers.at(poller_idx)->Poll();
    const double max = maybe_max != kMinValue ? maybe_max : myNaN;
    auto now = clock_.WallTime();
    auto stepMillis = poller_frequency_[poller_idx];
    auto offset = now % stepMillis;
    auto start_step = now - offset;
    return Measurements{Measurement{id_, start_step, max}};
  }

  void UpdatePollers() override {
    auto new_size = poller_frequency_.size();
    if (new_size > current_size_) {
      for (auto i = current_size_; i < new_size; ++i) {
        step_numbers.at(i) =
            new StepNumber<T>(kMinValue, poller_frequency_[i], clock_);
      }
    }
    current_size_ = new_size;
  }

 private:
  std::size_t current_size_;  // MUST be < MAX_POLLER_FREQ
  std::atomic<T> value_;      // to keep the local max

  mutable StepNumbers step_numbers;
  const Pollers& poller_frequency_;
};

using SubscriptionMaxGaugeInt = SubscriptionMaxGauge<int64_t>;

}  // namespace meter
}  // namespace atlas
