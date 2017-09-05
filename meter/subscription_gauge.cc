#include "subscription_gauge.h"

namespace atlas {
namespace meter {
Measurements SubscriptionGauge::MeasuresForPoller(size_t /*poller_idx*/) const {
  return Measurements{Measurement{id_, clock_.WallTime(), Value()}};
}

void SubscriptionGauge::UpdatePollers() {}

SubscriptionGauge::SubscriptionGauge(IdPtr id, const Clock& clock)
    : Meter(WithDefaultGaugeTags(id), clock), value_(myNaN) {}

double SubscriptionGauge::Value() const noexcept { return value_; }

std::ostream& SubscriptionGauge::Dump(std::ostream& os) const {
  os << "Gauge{" << *id_ << ", value=" << Value() << "}";
  return os;
}

void SubscriptionGauge::Update(double d) noexcept {
  value_.store(d, std::memory_order_relaxed);
  Updated();
}

}  // namespace meter
}  // namespace atlas
