
#include "monotonic_counter.h"
#include "statistic.h"

namespace atlas {
namespace meter {

MonotonicCounter::MonotonicCounter(Registry* registry, const IdPtr& id)
    : Meter{WithDefaultTagForId(id, statistic::count), registry->clock()},
      value_(0),
      registry_(registry),
      counter_(std::shared_ptr<Counter>(nullptr)) {}

std::ostream& MonotonicCounter::Dump(std::ostream& os) const {
  os << "MonotonicCounter("
     << "id=" << *id_ << ", value=" << value_
     << ", last_updated=" << last_updated_ << ")";

  return os;
}

Measurements MonotonicCounter::Measure() const { return Measurements(); }

void MonotonicCounter::Set(int64_t amount) noexcept {
  auto prev_updated = last_updated_.load(std::memory_order_relaxed);

  if (prev_updated > 0) {
    if (!counter_) {
      counter_ = registry_->counter(id_);
    }
    auto prev_value = value_.load(std::memory_order_relaxed);
    auto delta = amount - prev_value;
    if (delta >= 0) {
      counter_->Add(delta);
    }
  }
  value_.store(amount, std::memory_order_relaxed);
  Updated();
}

int64_t MonotonicCounter::Count() const noexcept {
  return value_.load(std::memory_order_relaxed);
}

}  // namespace meter
}  // namespace atlas
