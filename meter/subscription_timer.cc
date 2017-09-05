#include "subscription_timer.h"

#include "statistic.h"

namespace atlas {
namespace meter {

SubscriptionTimer::SubscriptionTimer(IdPtr id, const Clock& clock,
                                     Pollers& poller_frequency)
    : Meter{id, clock},
      current_size_(0),
      poller_frequency_(poller_frequency),
      count_(0),
      total_time_(0),
      sub_count_(id->WithTag(statistic::count), clock, poller_frequency),
      sub_total_sq_(id->WithTag(statistic::totalOfSquares), clock,
                    poller_frequency),
      sub_total_time_(id->WithTag(statistic::totalTime), clock,
                      poller_frequency),
      sub_max_(id->WithTag(statistic::max), clock, poller_frequency) {
  SubscriptionTimer::UpdatePollers();
  Updated();  // start the expiration timer
}

void SubscriptionTimer::Record(std::chrono::nanoseconds nanos) {
  const auto nanos_count = nanos.count();
  sub_count_.Increment();
  ++count_;
  if (nanos_count >= 0) {
    total_time_ += nanos_count;
    sub_total_time_.Add(nanos_count);
    auto nanos_sq =
        static_cast<double>(nanos_count) * static_cast<double>(nanos_count);
    sub_total_sq_.Add(nanos_sq);
    sub_max_.Update(nanos_count);
    Updated();
  }
}

int64_t SubscriptionTimer::Count() const noexcept {
  return count_.load(std::memory_order_relaxed);
}

int64_t SubscriptionTimer::TotalTime() const noexcept {
  return total_time_.load(std::memory_order_relaxed);
}

std::ostream& SubscriptionTimer::Dump(std::ostream& os) const {
  os << "SubscriptionTimer{";
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

Measurements SubscriptionTimer::MeasuresForPoller(size_t poller_idx) const {
  if (current_size_ <= poller_idx) {
    return Measurements();
  }

  auto count_measurement = sub_count_.MeasuresForPoller(poller_idx)[0];
  auto total_time_measurement = factor_measurement(
      sub_total_time_.MeasuresForPoller(poller_idx)[0], kCnvSeconds);
  auto total_sq_measurement = factor_measurement(
      sub_total_sq_.MeasuresForPoller(poller_idx)[0], kCnvSquares);
  auto max_measurement = factor_measurement(
      sub_max_.MeasuresForPoller(poller_idx)[0], kCnvSeconds);
  return Measurements{count_measurement, total_time_measurement,
                      total_sq_measurement, max_measurement};
}

void SubscriptionTimer::UpdatePollers() {
  sub_total_time_.UpdatePollers();
  sub_total_sq_.UpdatePollers();
  sub_count_.UpdatePollers();
  sub_max_.UpdatePollers();
  current_size_ = poller_frequency_.size();
}
}  // namespace meter
}  // namespace atlas
