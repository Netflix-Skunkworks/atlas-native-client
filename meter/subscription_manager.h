#pragma once

#include "atlas_registry.h"
#include "../util/config_manager.h"
#include "../util/http.h"
#include "../interpreter/evaluator.h"
#include "publisher.h"
#include "aggregate_registry.h"
#include <condition_variable>
#include <mutex>
#include <set>

namespace atlas {
namespace meter {

using ParsedSubscriptions = std::array<Subscriptions, util::kMainMultiple>;

class SubscriptionManager {
 public:
  explicit SubscriptionManager(
      const util::ConfigManager& config_manager) noexcept;
  ~SubscriptionManager();

  SubscriptionManager(const SubscriptionManager&) = delete;

  SubscriptionManager& operator=(const SubscriptionManager&) = delete;

  void Start() noexcept;

  void Stop(SystemClockWithOffset* clock = nullptr) noexcept;

  void PushMeasurements(int64_t now_millis,
                        const interpreter::TagsValuePairs& measurements) const;

  std::shared_ptr<Registry> GetRegistry() { return registry_; }

 private:
  interpreter::Evaluator evaluator_;
  const util::ConfigManager& config_manager_;
  std::shared_ptr<Registry> registry_;  // 5s registry
  AggregateRegistry main_registry_;
  Publisher publisher_;

  std::string current_etag;

  mutable std::mutex subscriptions_mutex;
  ParsedSubscriptions subscriptions_;
  std::array<std::unique_ptr<AggregateRegistry>, util::kMainMultiple>
      sub_registries_;

  std::atomic<bool> should_run_{false};
  std::thread sender_thread;
  std::thread sub_refresher_thread;
  std::mutex mutex;
  std::condition_variable cv;

  Subscriptions subs_for_frequency(size_t sub_idx) const noexcept;
  SubscriptionResults get_lwc_metrics(const Tags* common_tags,
                                      size_t sub_idx) const noexcept;
  void sub_refresher() noexcept;
  void main_sender() noexcept;
  void send_subscriptions(int64_t millis) noexcept;
  void update_metrics() noexcept;

 protected:
  // for testing
  void refresh_subscriptions(const std::string& subs_endpoint);
  void send_to_main();
};
}  // namespace meter
}  // namespace atlas
