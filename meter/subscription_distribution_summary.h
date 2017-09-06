#pragma once

#include "distribution_summary.h"
#include "meter.h"
#include "subscription_counter.h"
#include "subscription_max_gauge.h"

namespace atlas {
namespace meter {

template <typename T>
class SubscriptionDistributionSummaryNum : public Meter,
                                           public DistributionSummaryNumber<T> {
 public:
  SubscriptionDistributionSummaryNum(IdPtr id, const Clock& clock,
                                     Pollers& poller_frequency)
      : Meter(id, clock),
        current_size_(0),
        poller_frequency_(poller_frequency),
        count_(0),
        total_amount_(0),
        sub_count_(id->WithTag(statistic::count), clock, poller_frequency),
        sub_total_amount_(id->WithTag(statistic::totalAmount), clock,
                          poller_frequency),
        sub_total_sq_(id->WithTag(statistic::totalOfSquares), clock,
                      poller_frequency),
        sub_max_(id->WithTag(statistic::max), clock, poller_frequency) {
    Updated();
    SubscriptionDistributionSummaryNum::UpdatePollers();
  }

  Measurements MeasuresForPoller(size_t poller_idx) const override {
    if (current_size_ <= poller_idx) {
      return Measurements();
    }

    auto count_measurement = sub_count_.MeasuresForPoller(poller_idx)[0];
    auto total_amount_measurement =
        sub_total_amount_.MeasuresForPoller(poller_idx)[0];
    auto total_sq_measurement = sub_total_sq_.MeasuresForPoller(poller_idx)[0];
    auto max_measurement = sub_max_.MeasuresForPoller(poller_idx)[0];
    return Measurements{count_measurement, total_amount_measurement,
                        total_sq_measurement, max_measurement};
  }

  void UpdatePollers() override {
    sub_total_amount_.UpdatePollers();
    sub_total_sq_.UpdatePollers();
    sub_count_.UpdatePollers();
    sub_max_.UpdatePollers();
    current_size_ = poller_frequency_.size();
  }

  std::ostream& Dump(std::ostream& os) const override {
    os << "SubscriptionDistSummary{";
    sub_count_.Dump(os) << ", ";
    sub_total_amount_.Dump(os) << ", ";
    sub_total_sq_.Dump(os) << ", ";
    sub_max_.Dump(os) << ", ";
    return os;
  }

  void Record(T amount) noexcept override {
    sub_count_.Increment();
    ++count_;
    if (amount >= 0) {
      total_amount_ += amount;
      sub_total_amount_.Add(amount);
      auto amount_sq =
          static_cast<double>(amount) * static_cast<double>(amount);
      sub_total_sq_.Add(amount_sq);
      sub_max_.Update(amount);
      Updated();
    }
  }

  virtual int64_t Count() const noexcept override {
    return count_.load(std::memory_order_relaxed);
  }

  virtual T TotalAmount() const noexcept override {
    return total_amount_.load(std::memory_order_relaxed);
  }

 private:
  std::size_t current_size_;  // number of frequencies of subscribed clients
  const Pollers& poller_frequency_;

  // used to conform to the API for distribution summary
  std::atomic<int64_t> count_;
  std::atomic<int64_t> total_amount_;

  SubscriptionCounterNumber<int64_t> sub_count_;
  SubscriptionCounterNumber<T> sub_total_amount_;
  SubscriptionDoubleCounter sub_total_sq_;
  SubscriptionMaxGauge<T> sub_max_;
};

using SubscriptionDistributionSummary =
    SubscriptionDistributionSummaryNum<int64_t>;
using SubscriptionDoubleDistributionSummary =
    SubscriptionDistributionSummaryNum<double>;

}  // namespace meter
}  // namespace atlas
