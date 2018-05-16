#pragma once

#include "gauge.h"

namespace atlas {
namespace meter {

class DefaultGauge : public Meter, public Gauge<double> {
 public:
  DefaultGauge(const IdPtr& id, const Clock& clock);

  void Update(double d) noexcept override;
  double Value() const noexcept override;

  std::ostream& Dump(std::ostream& os) const override;

  Measurements Measure() const override;

 private:
  std::atomic<double> value_;
};
}  // namespace meter
}  // namespace atlas
