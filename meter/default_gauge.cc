#include "default_gauge.h"

namespace atlas {
namespace meter {

DefaultGauge::DefaultGauge(IdPtr id, const Clock& clock)
    : Meter(WithDefaultGaugeTags(id), clock), value_(myNaN) {}

double DefaultGauge::Value() const noexcept { return value_; }

std::ostream& DefaultGauge::Dump(std::ostream& os) const {
  os << "Gauge{" << *id_ << ", value=" << Value() << "}";
  return os;
}

Measurements DefaultGauge::Measure() const {
  return Measurements{Measurement{id_, clock_.WallTime(), Value()}};
}

void DefaultGauge::Update(double d) noexcept {
  value_.store(d, std::memory_order_relaxed);
  Updated();
}

}  // namespace meter
}  // namespace atlas
