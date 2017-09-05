#include "statistic.h"

#include "subscription_distribution_summary.h"

namespace atlas {
namespace meter {

void SubscriptionDistributionSummary::Record(int64_t amount) noexcept {
  sub_count_.Increment();
  ++count_;
  if (amount >= 0) {
    total_amount_ += amount;
    sub_total_amount_.Add(amount);
    auto amount_sq = static_cast<double>(amount) * static_cast<double>(amount);
    sub_total_sq_.Add(amount_sq);
    sub_max_.Update(amount);
    Updated();
  }
}

int64_t SubscriptionDistributionSummary::Count() const noexcept {
  return count_.load(std::memory_order_relaxed);
}

int64_t SubscriptionDistributionSummary::TotalAmount() const noexcept {
  return total_amount_.load(std::memory_order_relaxed);
}

SubscriptionDistributionSummary::SubscriptionDistributionSummary(
    IdPtr id, const Clock& clock, Pollers& poller_frequency)
    : Meter(id, clock),
      current_size_(0),
      poller_frequency_(poller_frequency),
      count_(0),
      total_amount_(0),
      sub_count_(id->WithTag(statistic::count), clock, poller_frequency),
      sub_total_amount_(id->WithTag(statistic::totalAmount), clock,
                        poller_frequency),
      sub_total_sq_(id->WithTag(statistic::totalOfSquares), clock,
                    poller_frequency),
      sub_max_(id->WithTag(statistic::max), clock, poller_frequency) {
  Updated();
  SubscriptionDistributionSummary::UpdatePollers();
}

Measurements SubscriptionDistributionSummary::MeasuresForPoller(
    size_t poller_idx) const {
  if (current_size_ <= poller_idx) {
    return Measurements();
  }

  auto count_measurement = sub_count_.MeasuresForPoller(poller_idx)[0];
  auto total_amount_measurement =
      sub_total_amount_.MeasuresForPoller(poller_idx)[0];
  auto total_sq_measurement = sub_total_sq_.MeasuresForPoller(poller_idx)[0];
  auto max_measurement = sub_max_.MeasuresForPoller(poller_idx)[0];
  return Measurements{count_measurement, total_amount_measurement,
                      total_sq_measurement, max_measurement};
}

void SubscriptionDistributionSummary::UpdatePollers() {
  sub_total_amount_.UpdatePollers();
  sub_total_sq_.UpdatePollers();
  sub_count_.UpdatePollers();
  sub_max_.UpdatePollers();
  current_size_ = poller_frequency_.size();
}

std::ostream& SubscriptionDistributionSummary::Dump(std::ostream& os) const {
  os << "SubscriptionDistSummary{";
  sub_count_.Dump(os) << ", ";
  sub_total_amount_.Dump(os) << ", ";
  sub_total_sq_.Dump(os) << ", ";
  sub_max_.Dump(os) << ", ";
  return os;
}
}  // namespace meter
}  // namespace atlas
