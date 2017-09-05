#include <array>
#include "counter.h"
#include "stepnumber.h"
#include "statistic.h"

#pragma once

namespace atlas {
namespace meter {

template <typename T>
class SubscriptionCounterNumber : public Meter, public CounterNumber<T> {
  template <typename CT>
  struct type {};

 public:
  SubscriptionCounterNumber(IdPtr id, const Clock& clock,
                            Pollers& poller_frequency)
      : Meter(WithDefaultTagForId(id, statistic::count), clock),
        current_size_(0),
        value_(0),
        poller_frequency_(poller_frequency) {
    Updated();
    UpdatePollers();
  }

  std::ostream& Dump(std::ostream& os) const override {
    os << "SubscriptionCounter(id=" << *id_ << ", ";
    if (current_size_ > 0) {
      os << poller_frequency_[0] << "s=" << step_numbers_[0]->Current();
      for (auto i = 0u; i < current_size_; ++i) {
        os << poller_frequency_[0] << ", s=" << step_numbers_[0]->Current();
      }
    }
    os << ")";
    return os;
  }

  Measurements MeasuresForPoller(size_t poller_idx) const override {
    if (current_size_ <= poller_idx) {
      return Measurements();
    }
    auto poller_freq_secs = poller_frequency_[poller_idx] / 1000.0;

    auto& sl = step_numbers_[poller_idx];
    auto rate = sl->Poll() / poller_freq_secs;
    auto now = clock_.WallTime();
    auto stepMillis = poller_frequency_[poller_idx];
    auto offset = now % stepMillis;
    auto start_step = now - offset;
    return Measurements{Measurement{id_, start_step, rate}};
  }

  void Increment() noexcept override { Add(1); }

  void Add(T amount) noexcept override {
    UpdateSteps(amount);
    Add(amount, type<T>());
  }

  T Count() const noexcept override {
    return value_.load(std::memory_order_relaxed);
  }

  void UpdatePollers() override {
    auto new_size = poller_frequency_.size();
    if (new_size > current_size_) {
      for (auto i = current_size_; i < new_size; ++i) {
        step_numbers_[i] = new StepNumber<T>(0, poller_frequency_[i], clock_);
      }
    }
    current_size_ = new_size;
  }

 private:
  std::size_t current_size_;  // MUST be < MAX_POLLER_FREQ

  using StepNumbers = std::array<StepNumber<T>*, MAX_POLLER_FREQ>;
  mutable StepNumbers step_numbers_;
  std::atomic<T> value_;  // to keep the total count
  const Pollers& poller_frequency_;

  void UpdateSteps(T amount) {
    for (auto i = 0u; i < current_size_; ++i) {
      step_numbers_[i]->Add(amount);
    }
    Updated();
  }

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

using SubscriptionCounter = SubscriptionCounterNumber<int64_t>;
using SubscriptionDoubleCounter = SubscriptionCounterNumber<double>;
}  // namespace meter
}  // namespace atlas
