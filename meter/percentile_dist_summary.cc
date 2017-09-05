#include "percentile_dist_summary.h"
#include "statistic.h"

namespace atlas {
namespace meter {

#include "percentile_buckets_tags_generated.inc"

PercentileDistributionSummary::PercentileDistributionSummary(Registry* registry,
                                                             IdPtr id)
    : Meter{id, registry->clock()},
      registry_{registry},
      dist_{registry->distribution_summary(id)} {}

std::ostream& PercentileDistributionSummary::Dump(std::ostream& os) const {
  os << "PercentileDistributionSummary{" << id_ << "}";
  return os;
}

void PercentileDistributionSummary::Record(int64_t amount) noexcept {
  dist_->Record(amount);
  CounterFor(percentile_buckets::IndexOf(amount))->Increment();
}

const std::string kPercentile{"percentile"};

inline Tag PercentileTag(size_t i) { return Tag{kPercentile, kDistTags.at(i)}; }

std::shared_ptr<Counter> PercentileDistributionSummary::CounterFor(
    size_t i) const noexcept {
  auto& c = counters_.at(i);
  if (!c) {
    c = registry_->counter(id_->WithTag(statistic::percentile)->WithTag(PercentileTag(i)));
    counters_.at(i) = c;
  }
  return c;
}

double PercentileDistributionSummary::Percentile(double p) const noexcept {
  std::array<int64_t, percentile_buckets::Length()> counts;
  for (size_t i = 0; i < percentile_buckets::Length(); ++i) {
    counts.at(i) = CounterFor(i)->Count();
  }
  return percentile_buckets::Percentile(counts, p);
}

}  // namespace meter
}  // namespace atlas
