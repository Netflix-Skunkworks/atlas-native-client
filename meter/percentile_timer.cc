#include "percentile_timer.h"
#include "statistic.h"

namespace atlas {
namespace meter {

#include "percentile_bucket_tags.inc"

PercentileTimer::PercentileTimer(Registry* registry, const IdPtr& id)
    : Meter{id, registry->clock()},
      registry_{registry},
      timer_{registry->timer(id)} {}

std::ostream& PercentileTimer::Dump(std::ostream& os) const {
  os << "PercentileTimer{" << id_ << "}";
  return os;
}

void PercentileTimer::Record(std::chrono::nanoseconds nanos) noexcept {
  timer_->Record(nanos);
  CounterFor(percentile_buckets::IndexOf(nanos.count()))->Increment();
}

inline Tag PercentileTag(size_t i) {
  static std::string kPercentile{"percentile"};
  return Tag::of(kPercentile, kTimerTags.at(i));
}

std::shared_ptr<Counter> PercentileTimer::CounterFor(size_t i) const noexcept {
  auto& c = counters_.at(i);
  if (!c) {
    c = registry_->counter(
        id_->WithTag(statistic::percentile)->WithTag(PercentileTag(i)));
    counters_.at(i) = c;
  }
  return c;
}

double PercentileTimer::Percentile(double p) const noexcept {
  std::array<int64_t, percentile_buckets::Length()> counts{};
  for (size_t i = 0; i < percentile_buckets::Length(); ++i) {
    counts.at(i) = CounterFor(i)->Count();
  }
  auto v = percentile_buckets::Percentile(counts, p);
  return v / 1e9;
}

}  // namespace meter
}  // namespace atlas
