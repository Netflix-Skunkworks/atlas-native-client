#pragma once

#include "distribution_summary.h"
#include "meter.h"
#include "percentile_buckets.h"
#include "registry.h"

#include <array>

namespace atlas {
namespace meter {

class PercentileDistributionSummary : public Meter, public DistributionSummary {
 public:
  PercentileDistributionSummary(Registry* registry, const IdPtr& id);
  void Record(int64_t amount) noexcept override;
  std::ostream& Dump(std::ostream& os) const override;
  Measurements Measure() const override { return Measurements(); };
  int64_t Count() const noexcept override { return dist_->Count(); }
  int64_t TotalAmount() const noexcept override { return dist_->TotalAmount(); }
  double Percentile(double p) const noexcept;

 private:
  Registry* registry_;
  std::shared_ptr<DistributionSummary> dist_;
  mutable std::array<std::shared_ptr<Counter>, percentile_buckets::Length()>
      counters_;
  std::shared_ptr<Counter> CounterFor(size_t i) const noexcept;
};

}  // namespace meter
}  // namespace atlas
