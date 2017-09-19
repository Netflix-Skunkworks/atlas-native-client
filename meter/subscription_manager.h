#pragma once

#include "../util/config_manager.h"
#include "subscription_registry.h"
#include <set>

namespace atlas {
namespace meter {

class SubscriptionManager {
 public:
  SubscriptionManager(const util::ConfigManager& config_manager,
                      SubscriptionRegistry& registry);

  ~SubscriptionManager();

  SubscriptionManager(const SubscriptionManager&) = delete;

  SubscriptionManager& operator=(const SubscriptionManager&) = delete;

  void Start() noexcept;

  void Stop(SystemClockWithOffset* clock = nullptr) noexcept;

  void PushMeasurements(int64_t now_millis,
                        const interpreter::TagsValuePairs& measurements) const;

 private:
  const util::ConfigManager& config_manager_;
  std::set<int> sender_intervals_;

  std::string current_etag;
  // unfortunately no atomic unique ptrs available yet,
  // so we use a raw pointer to the current subscriptions
  std::atomic<Subscriptions*> subscriptions_{nullptr};
  SubscriptionRegistry& registry_;
  std::atomic<bool> should_run_{false};

  void SubRefresher() noexcept;
  void MainSender(std::chrono::seconds initial_delay) noexcept;
  void SubSender(int64_t millis) noexcept;
  void SendMetricsForInterval(int64_t millis) noexcept;

 protected:
  // for testing
  void RefreshSubscriptions(const std::string& subs_endpoint);

  void SendToMain();

  void UpdateMetrics() noexcept;
};
}  // namespace meter
}  // namespace atlas
