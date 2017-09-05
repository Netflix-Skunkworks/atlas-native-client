#pragma once

#include "subscription_counter.h"
#include "subscription_max_gauge.h"
#include "timer.h"

namespace atlas {
namespace meter {

class SubscriptionTimer : public Meter, public Timer {
 public:
  SubscriptionTimer(IdPtr id, const Clock& clock, Pollers& poller_frequency);

  Measurements MeasuresForPoller(size_t poller_idx) const override;

  void UpdatePollers() override;

  std::ostream& Dump(std::ostream& os) const override;

  void Record(std::chrono::nanoseconds nanos) override;

  int64_t Count() const noexcept override;

  int64_t TotalTime() const noexcept override;

 private:
  std::size_t current_size_;  // number of frequencies of subscribed clients
  const Pollers& poller_frequency_;

  // used to conform to the API for timer
  std::atomic<int64_t> count_;
  std::atomic<int64_t> total_time_;

  SubscriptionCounter sub_count_;
  SubscriptionDoubleCounter sub_total_sq_;
  SubscriptionCounter sub_total_time_;
  SubscriptionMaxGaugeInt sub_max_;
};
}  // namespace meter
}  // namespace atlas
