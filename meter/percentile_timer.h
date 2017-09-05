#pragma once

#include <array>
#include "meter.h"
#include "percentile_buckets.h"
#include "registry.h"
#include "timer.h"

namespace atlas {
namespace meter {

class PercentileTimer : public Meter, public Timer {
 public:
  PercentileTimer(Registry* registry, IdPtr id);
  void Record(std::chrono::nanoseconds nanos) noexcept override;
  std::ostream& Dump(std::ostream& os) const override;
  Measurements MeasuresForPoller(size_t) const override {
    return Measurements();
  };
  int64_t Count() const noexcept override { return timer_->Count(); }
  int64_t TotalTime() const noexcept override { return timer_->TotalTime(); }
  double Percentile(double p) const noexcept;

 private:
  Registry* registry_;
  std::shared_ptr<Timer> timer_;
  mutable std::array<std::shared_ptr<Counter>, percentile_buckets::Length()>
      counters_;
  std::shared_ptr<Counter> CounterFor(size_t i) const noexcept;
};

}  // namespace meter
}  // namespace atlas
