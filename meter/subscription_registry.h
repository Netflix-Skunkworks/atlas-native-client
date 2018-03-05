#pragma once

#include "../util/config.h"
#include "registry.h"
#include "stepnumber.h"
#include "subscription.h"
#include "subscription_max_gauge.h"
#include <array>

namespace atlas {

namespace interpreter {
class Interpreter;
}  // namespace interpreter

namespace meter {

extern SystemClock system_clock;

class SubscriptionRegistry : public Registry {
  class impl;
  std::unique_ptr<impl> impl_;

 public:
  explicit SubscriptionRegistry(
      std::unique_ptr<interpreter::Interpreter> interpreter,
      const Clock* clock = &system_clock) noexcept;

  SubscriptionRegistry(const SubscriptionRegistry& registry) = delete;
  virtual ~SubscriptionRegistry();

  const Clock& clock() const noexcept override;

  Pollers& pollers() noexcept override;

  IdPtr CreateId(std::string name, Tags tags) override;

  std::shared_ptr<Counter> counter(IdPtr id) noexcept override;

  std::shared_ptr<DCounter> dcounter(IdPtr id) noexcept override;

  std::shared_ptr<Timer> timer(IdPtr id) noexcept override;

  std::shared_ptr<Gauge<double>> gauge(IdPtr id) noexcept override;

  std::shared_ptr<LongTaskTimer> long_task_timer(IdPtr id) noexcept override;

  std::shared_ptr<DistributionSummary> distribution_summary(
      IdPtr id) noexcept override;

  void RegisterMonitor(std::shared_ptr<Meter> meter) noexcept override;

  Meters meters() const noexcept override;

  void update_subscriptions(const Subscriptions* new_subs);

  SubscriptionResults GetLwcMetricsForInterval(const util::Config& config,
                                               int64_t frequency) const;

  std::unique_ptr<interpreter::TagsValuePairs> GetMainMeasurements(
      const util::Config& config, const Tags& common_tags) const;

  std::shared_ptr<Counter> counter(std::string name) {
    return counter(CreateId(name, kEmptyTags));
  }

  std::shared_ptr<Timer> timer(std::string name) {
    return timer(CreateId(name, kEmptyTags));
  }

  std::shared_ptr<Gauge<double>> max_gauge(IdPtr id) noexcept override {
    return CreateAndRegisterAsNeeded<SubscriptionMaxGauge<double>>(id);
  }

  std::shared_ptr<Gauge<double>> gauge(std::string name) {
    return gauge(CreateId(name, kEmptyTags));
  }

  std::shared_ptr<LongTaskTimer> long_task_timer(std::string name) {
    return long_task_timer(CreateId(name, kEmptyTags));
  }

  std::shared_ptr<DistributionSummary> distribution_summary(std::string name) {
    return distribution_summary(CreateId(name, kEmptyTags));
  }

 private:
  const Clock* clock_;

  // guarantee that only one thread will call update_subscription
  mutable std::mutex subscriptions_mutex;

  std::shared_ptr<Meter> InsertIfNeeded(std::shared_ptr<Meter> meter) noexcept;
  std::shared_ptr<Meter> GetMeter(IdPtr id) noexcept;

  // FIXME remove code duplication - difference is constructor takes
  // poller_freq_
  template <typename M>
  std::shared_ptr<M> CreateAndRegisterAsNeededG(IdPtr id) noexcept {
    auto existing = GetMeter(id);
    if (existing) {
      return std::static_pointer_cast<M>(existing);
    }
    std::shared_ptr<M> new_meter_ptr{
        std::make_shared<M>(std::move(id), clock())};
    auto meter_ptr = InsertIfNeeded(new_meter_ptr);
    return std::static_pointer_cast<M>(meter_ptr);
  }

  template <typename M>
  std::shared_ptr<M> CreateAndRegisterAsNeeded(IdPtr id) noexcept {
    auto existing = GetMeter(id);
    if (existing) {
      return std::static_pointer_cast<M>(existing);
    }
    std::shared_ptr<M> new_meter_ptr{
        std::make_shared<M>(id, clock(), poller_freq_)};
    auto meter_ptr = InsertIfNeeded(new_meter_ptr);
    return std::static_pointer_cast<M>(meter_ptr);
  }

  Pollers poller_freq_;

  const Subscriptions* subscriptions_{nullptr};

  bool AlreadySeen(int s) noexcept;

 protected:  // for testing
  Subscriptions SubsForInterval(int64_t frequency) const noexcept;

  Measurements GetMeasurements(int64_t frequency) const;

  std::shared_ptr<interpreter::TagsValuePairs> evaluate(
      const std::string& expression,
      std::shared_ptr<interpreter::TagsValuePairs> tagsValuePairs) const;
};
}  // namespace meter
}  // namespace atlas
