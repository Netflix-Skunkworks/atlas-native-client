#include "interval_counter.h"
#include "statistic.h"

namespace atlas {
namespace meter {

static std::function<double(int64_t)> age(const Clock& clock) {
  return [&clock](int64_t t) {
    return static_cast<double>(clock.WallTime() - t) / 1000.0;
  };
}

IntervalCounter::IntervalCounter(Registry* registry, const IdPtr& id)
    : Meter{id, registry->clock()},
      counter_{registry->counter(id->WithTag(statistic::count))},
      counter_updated{std::make_shared<int64_t>(0)},
      gauge_{std::make_shared<FunctionGauge<int64_t>>(
          id->WithTag(statistic::duration), registry->clock(), counter_updated,
          age(registry->clock()))} {
  registry->RegisterMonitor(gauge_);
}

void IntervalCounter::Increment() noexcept {
  counter_->Increment();
  *counter_updated = clock_.WallTime();
}

void IntervalCounter::Add(int64_t amount) noexcept {
  counter_->Add(amount);
  *counter_updated = clock_.WallTime();
}

int64_t IntervalCounter::Count() const noexcept { return counter_->Count(); }

Measurements IntervalCounter::Measure() const { return Measurements{}; }

std::ostream& IntervalCounter::Dump(std::ostream& os) const {
  os << "IntervalCounter{" << id_ << ",count=" << counter_->Count()
     << ",updated=" << counter_updated << "}";
  return os;
}

double IntervalCounter::SecondsSinceLastUpdate() const noexcept {
  return (clock_.WallTime() - *counter_updated) / 1000.0;
}

}  // namespace meter
}  // namespace atlas
