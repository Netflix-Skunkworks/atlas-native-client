#include "default_timer.h"

#include "statistic.h"

namespace atlas {
namespace meter {

DefaultTimer::DefaultTimer(const IdPtr& id, const Clock& clock,
                           int64_t freq_millis)
    : Meter{id, clock},
      count_(0),
      total_time_(0),
      sub_count_(id->WithTag(statistic::count), clock, freq_millis),
      sub_total_sq_(id->WithTag(statistic::totalOfSquares), clock, freq_millis),
      sub_total_time_(id->WithTag(statistic::totalTime), clock, freq_millis),
      sub_max_(id->WithTag(statistic::max), clock, freq_millis) {
  Updated();  // start the expiration timer
}

void DefaultTimer::Record(std::chrono::nanoseconds nanos) {
  const auto nanos_count = nanos.count();
  if (nanos_count >= 0) {
    sub_count_.Increment();
    ++count_;
    total_time_ += nanos_count;
    sub_total_time_.Add(nanos_count);
    auto nanos_sq =
        static_cast<double>(nanos_count) * static_cast<double>(nanos_count);
    sub_total_sq_.Add(nanos_sq);
    sub_max_.Update(nanos_count);
    Updated();
  }
}

int64_t DefaultTimer::Count() const noexcept {
  return count_.load(std::memory_order_relaxed);
}

int64_t DefaultTimer::TotalTime() const noexcept {
  return total_time_.load(std::memory_order_relaxed);
}

std::ostream& DefaultTimer::Dump(std::ostream& os) const {
  os << "DefaultTimer{";
  sub_count_.Dump(os) << ", ";
  sub_total_time_.Dump(os) << ", ";
  sub_total_sq_.Dump(os) << ", ";
  sub_max_.Dump(os) << "}";
  return os;
}

static constexpr auto kCnvSeconds =
    1.0 / 1e9;  // factor to convert nanos to seconds
static constexpr auto kCnvSquares =
    kCnvSeconds * kCnvSeconds;  // factor to convert nanos squared to seconds

Measurements DefaultTimer::Measure() const {
  auto count_measurement = sub_count_.Measure()[0];
  auto total_time_measurement =
      factor_measurement(sub_total_time_.Measure()[0], kCnvSeconds);
  auto total_sq_measurement =
      factor_measurement(sub_total_sq_.Measure()[0], kCnvSquares);
  auto max_measurement = factor_measurement(sub_max_.Measure()[0], kCnvSeconds);
  return Measurements{count_measurement, total_time_measurement,
                      total_sq_measurement, max_measurement};
}

}  // namespace meter
}  // namespace atlas
