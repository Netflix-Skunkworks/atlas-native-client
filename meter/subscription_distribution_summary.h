#pragma once

#include "distribution_summary.h"
#include "meter.h"
#include "subscription_counter.h"
#include "subscription_max_gauge.h"

namespace atlas {
namespace meter {

class SubscriptionDistributionSummary : public Meter,
                                        public DistributionSummary {
 public:
  SubscriptionDistributionSummary(IdPtr id, const Clock& clock,
                                  Pollers& poller_frequency);

  virtual Measurements MeasuresForPoller(size_t poller_idx) const override;

  virtual void UpdatePollers() override;

  std::ostream& Dump(std::ostream& os) const override;

  virtual void Record(int64_t amount) noexcept override;

  virtual int64_t Count() const noexcept override;

  virtual int64_t TotalAmount() const noexcept override;

 private:
  std::size_t current_size_;  // number of frequencies of subscribed clients
  const Pollers& poller_frequency_;

  // used to conform to the API for distribution summary
  std::atomic<int64_t> count_;
  std::atomic<int64_t> total_amount_;

  SubscriptionCounter sub_count_;
  SubscriptionCounter sub_total_amount_;
  SubscriptionDoubleCounter sub_total_sq_;
  SubscriptionMaxGaugeInt sub_max_;
};
}  // namespace meter
}  // namespace atlas
