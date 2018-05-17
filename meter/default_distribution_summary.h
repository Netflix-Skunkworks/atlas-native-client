#pragma once

#include "distribution_summary.h"
#include "meter.h"
#include "default_counter.h"
#include "default_max_gauge.h"

namespace atlas {
namespace meter {

template <typename T>
class DefaultDistributionSummaryNum : public Meter,
                                      public DistributionSummaryNumber<T> {
 public:
  DefaultDistributionSummaryNum(IdPtr id, const Clock& clock,
                                int64_t freq_millis)
      : Meter(id, clock),
        count_(0),
        total_amount_(0),
        sub_count_(id->WithTag(statistic::count), clock, freq_millis),
        sub_total_amount_(id->WithTag(statistic::totalAmount), clock,
                          freq_millis),
        sub_total_sq_(id->WithTag(statistic::totalOfSquares), clock,
                      freq_millis),
        sub_max_(id->WithTag(statistic::max), clock, freq_millis) {
    Updated();
  }

  Measurements Measure() const override {
    auto count_measurement = sub_count_.Measure()[0];
    auto total_amount_measurement = sub_total_amount_.Measure()[0];
    auto total_sq_measurement = sub_total_sq_.Measure()[0];
    auto max_measurement = sub_max_.Measure()[0];
    return Measurements{count_measurement, total_amount_measurement,
                        total_sq_measurement, max_measurement};
  }

  std::ostream& Dump(std::ostream& os) const override {
    os << "DefaultDistSummary{";
    sub_count_.Dump(os) << ", ";
    sub_total_amount_.Dump(os) << ", ";
    sub_total_sq_.Dump(os) << ", ";
    sub_max_.Dump(os) << "}";
    return os;
  }

  void Record(T amount) noexcept override {
    if (amount >= 0) {
      sub_count_.Increment();
      ++count_;
      total_amount_ += amount;
      sub_total_amount_.Add(amount);
      auto amount_sq =
          static_cast<double>(amount) * static_cast<double>(amount);
      sub_total_sq_.Add(amount_sq);
      sub_max_.Update(amount);
    }
    Updated();
  }

  int64_t Count() const noexcept override {
    return count_.load(std::memory_order_relaxed);
  }

  T TotalAmount() const noexcept override {
    return total_amount_.load(std::memory_order_relaxed);
  }

 private:
  // used to conform to the API for distribution summary
  std::atomic<int64_t> count_;
  std::atomic<int64_t> total_amount_;

  DefaultCounterNumber<int64_t> sub_count_;
  DefaultCounterNumber<T> sub_total_amount_;
  DefaultDoubleCounter sub_total_sq_;
  DefaultMaxGauge<T> sub_max_;
};

using DefaultDistributionSummary = DefaultDistributionSummaryNum<int64_t>;
using DefaultDoubleDistributionSummary = DefaultDistributionSummaryNum<double>;

}  // namespace meter
}  // namespace atlas
