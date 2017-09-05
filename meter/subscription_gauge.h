#pragma once

#include "gauge.h"

namespace atlas {
namespace meter {

class SubscriptionGauge : public Meter, public Gauge<double> {
 public:
  SubscriptionGauge(IdPtr id, const Clock& clock);

  void Update(double d) noexcept override;
  double Value() const noexcept override;

  std::ostream& Dump(std::ostream& os) const override;

  Measurements MeasuresForPoller(size_t poller_idx) const override;

  void UpdatePollers() override;

 private:
  std::atomic<double> value_;
};
}  // namespace meter
}  // namespace atlas
