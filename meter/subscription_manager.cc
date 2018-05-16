#include "subscription_manager.h"
#include "../atlas_client.h"
#include "../interpreter/group_by.h"
#include "../interpreter/query.h"
#include "../util/config.h"
#include "../util/http.h"
#include "../util/json.h"
#include "../util/logger.h"
#include "../util/string_pool.h"
#include "../util/strings.h"
#include "subscription_format.h"
#include "validation.h"
#include <chrono>
#include <cstdlib>
#include <curl/curl.h>
#include <fstream>
#include <random>
#include <sstream>
#include <zlib.h>

namespace atlas {
namespace meter {

using atlas::util::kMainFrequencyMillis;

SubscriptionManager::SubscriptionManager(
    const util::ConfigManager& config_manager) noexcept
    : config_manager_(config_manager),
      registry_(std::make_shared<AtlasRegistry>(kMainFrequencyMillis)),
      publisher_(registry_) {}

using interpreter::Evaluator;
using interpreter::TagsValuePairs;
using util::kMainFrequencyMillis;
using util::Logger;

using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::system_clock;

static constexpr int64_t kMinWaitMillis = 1;

void SubscriptionManager::main_sender(
    std::chrono::seconds initial_delay) noexcept {
  Logger()->info("Waiting for {} seconds to send the first batch to main.",
                 initial_delay.count());

  {
    std::unique_lock<std::mutex> lock{mutex};
    cv.wait_for(lock, initial_delay);
  }
  while (should_run_) {
    auto start = system_clock::now();
    const auto& config = config_manager_.GetConfig();
    if (config->IsMainEnabled()) {
      Logger()->debug("send_to_main()");
      try {
        update_metrics();
        send_to_main();
      } catch (const std::exception& e) {
        Logger()->error("Error sending to main publish cluster: {}", e.what());
      }
    } else {
      Logger()->info(
          "Not sending anything to the main publish cluster (disabled)");
    }

    if (config->AreSubsEnabled()) {
      Logger()->debug("sending subscriptions");
      try {
        send_subscriptions(kMainFrequencyMillis);
      } catch (const std::exception& e) {
        Logger()->error("Error sending sending subscriptions {}", e.what());
      }
    } else {
      Logger()->debug("subscriptions are disabled.");
    }
    auto elapsed_millis =
        duration_cast<milliseconds>(system_clock::now() - start).count();
    auto wait_millis =
        std::max(kMinWaitMillis, kMainFrequencyMillis - elapsed_millis);
    Logger()->debug("Waiting {} millis until next main publish sender run",
                    wait_millis);
    std::unique_lock<std::mutex> lock{mutex};
    cv.wait_for(lock, milliseconds(wait_millis));
  }
}

void SubscriptionManager::sub_refresher() noexcept {
  while (should_run_) {
    try {
      auto start = system_clock::now();
      const auto& config = config_manager_.GetConfig();
      const auto& endpoints = config->EndpointConfiguration();
      auto subs_endpoint = endpoints.subscriptions;
      if (config->AreSubsEnabled()) {
        Logger()->info("Refreshing subscriptions from {}", subs_endpoint);
        refresh_subscriptions(subs_endpoint);
      }
      auto end = system_clock::now();
      auto elapsed_millis = duration_cast<milliseconds>(end - start).count();
      auto refresh_millis = config->SubRefreshMillis();
      auto wait_millis =
          std::max(kMinWaitMillis, refresh_millis - elapsed_millis);
      Logger()->debug("Waiting {}ms for next refresh subscription cycle",
                      wait_millis);
      std::unique_lock<std::mutex> lock{mutex};
      cv.wait_for(lock, milliseconds(wait_millis));
    } catch (std::exception& e) {
      Logger()->info("Ignoring exception while refreshing configs: {}",
                     e.what());
    }
  }
}

Subscriptions* ParseSubscriptions(const std::string& subs_str) {
  using rapidjson::Document;
  using rapidjson::kParseCommentsFlag;
  using rapidjson::kParseNanAndInfFlag;

  Document document;
  document.Parse<kParseCommentsFlag | kParseNanAndInfFlag>(subs_str.c_str());
  auto subs = new Subscriptions;
  if (document.IsObject()) {
    auto expressions = document["expressions"].GetArray();
    for (auto& e : expressions) {
      if (e.IsObject()) {
        auto expr = e.GetObject();
        subs->emplace_back(Subscription{expr["id"].GetString(),
                                        expr["frequency"].GetInt64(),
                                        expr["expression"].GetString()});
      }
    }
  }
  Logger()->debug("Got subscriptions: {}", *subs);

  return subs;
}
void SubscriptionManager::refresh_subscriptions(
    const std::string& subs_endpoint) {
  auto timer_refresh_subs = registry_->timer("atlas.client.refreshSubs");

  std::string subs_str;
  const auto& logger = Logger();

  auto cfg = config_manager_.GetConfig();
  util::http http_client(registry_, cfg->HttpConfiguration());
  auto start = registry_->clock().MonotonicTime();
  auto http_res =
      http_client.conditional_get(subs_endpoint, &current_etag, &subs_str);
  timer_refresh_subs->Record(registry_->clock().MonotonicTime() - start);
  if (http_res == 200) {
    try {
      auto subscriptions = ParseSubscriptions(subs_str);
      auto old_subs = subscriptions_.exchange(subscriptions);
      delete old_subs;
    } catch (...) {
      auto refresh_parsing_errors = registry_->counter(registry_->CreateId(
          "atlas.client.refreshSubsErrors", Tags{{"error", "json"}}));
      refresh_parsing_errors->Increment();
    }
  } else if (http_res == 304) {
    logger->debug("Not refreshing subscriptions. Not modified");
  } else {
    logger->error("Failed to refresh subscriptions: {}", http_res);
    auto refresh_http_errors = registry_->counter(
        "atlas.client.refreshSubsErrors", Tags{{"error", "http"}});
    refresh_http_errors->Increment();
  }
}

void SubscriptionManager::Stop(SystemClockWithOffset* clock) noexcept {
  if (should_run_) {
    should_run_ = false;
    cv.notify_all();
    if (clock != nullptr) {
      Logger()->info("Advancing clock and flushing metrics");
      clock->SetOffset(59900);
      send_to_main();
    }
    Logger()->debug("Stopping main sender");
    main_sender_thread.join();
    Logger()->debug("Stopping sub_refresher");
    sub_refresher_thread.join();
  }
}

SubscriptionManager::~SubscriptionManager() {
  delete subscriptions_.load();
  if (should_run_) {
    Stop();
  }
}

static constexpr int kMaxSecsToStart = 20;
static constexpr int kMainFrequencySecs = kMainFrequencyMillis / 1000;
static std::chrono::seconds GetInitialDelay() {
  using std::chrono::seconds;
  std::random_device rd;
  int rand_seconds = rd();
  auto targetSecs = std::abs(rand_seconds % kMaxSecsToStart);
  auto now = system_clock::to_time_t(system_clock::now());
  auto offset = now % kMainFrequencySecs;
  auto delay = targetSecs - offset;
  return seconds(delay >= 0 ? delay : delay + kMainFrequencySecs);
}

void SubscriptionManager::Start() noexcept {
  should_run_ = true;
  sub_refresher_thread = std::thread(&SubscriptionManager::sub_refresher, this);
  auto initialDelay = GetInitialDelay();
  main_sender_thread =
      std::thread(&SubscriptionManager::main_sender, this, initialDelay);
}

Subscriptions SubscriptionManager::subs_for_frequency(int64_t frequency) const
    noexcept {
  //  std::lock_guard<std::mutex> guard(subscriptions_mutex);
  Subscriptions res;
  std::copy_if(
      std::begin(*subscriptions_), std::end(*subscriptions_),
      std::back_inserter(res), [frequency](const Subscription& subscription) {
        return subscription.frequency == frequency && !subscription.id.empty();
      });

  return res;
}

void SubscriptionManager::send_subscriptions(int64_t millis) noexcept {
  using util::secs_for_millis;

  const auto& clock = registry_->clock();
  const auto start = clock.MonotonicTime();
  auto timer = registry_->timer("atlas.client.sendLwc",
                                Tags{{"id", secs_for_millis(millis).c_str()}});
  auto cfg = config_manager_.GetConfig();
  auto common_tags = cfg->CommonTags();

  const auto& sub_results = get_lwc_metrics(&common_tags, millis);
  if (sub_results.empty()) {
    Logger()->debug("No subscription results found");
  } else {
    Logger()->debug("Sending {} subscription results", sub_results.size());
    publisher_.SendSubscriptions(*cfg, sub_results);
    timer->Record(clock.MonotonicTime() - start);
  }
}

void SubscriptionManager::update_metrics() noexcept {
  auto pool_size = registry_->gauge("atlas.client.strPoolSize");
  auto pool_alloc = registry_->gauge("atlas.client.strPoolAlloc");

  pool_size->Update(util::the_str_pool().pool_size());
  pool_alloc->Update(util::the_str_pool().alloc_size());
}

static void log_rules(const std::vector<std::string>& rules,
                      size_t num_measurements) {
  auto logger = Logger();
  if (logger->should_log(spdlog::level::debug)) {
    std::ostringstream os;
    os << "Applying [\n";
    bool first = true;
    for (const auto& rule : rules) {
      if (!first) {
        os << "\n";
      } else {
        first = false;
      }
      os << "    " << rule;
    }
    os << "] to " << num_measurements << " measurements";
    logger->debug(os.str());
  }
}

static void log_measures_for_rule(const std::string& rule,
                                  const interpreter::TagsValuePairs& pairs) {
  auto logger = Logger();
  if (logger->should_log(spdlog::level::debug)) {
    logger->debug("Rule: {}", rule);
    for (const auto& pair : pairs) {
      logger->debug("\t{}", *pair);
    }
  }
}

SubscriptionResults generate_sub_results(const Evaluator& evaluator,
                                         const Subscriptions& subs,
                                         const TagsValuePairs& pairs) {
  using interpreter::TagsValuePair;
  SubscriptionResults result;
  for (auto& s : subs) {
    auto expr_result = evaluator.eval(s.expression, pairs);
    for (const auto& pair : expr_result) {
      auto value = pair->value();
      if (!std::isnan(value)) {
        result.emplace_back(SubscriptionMetric{s.id, pair->all_tags(), value});
      }
    }
  }
  return result;
}

SubscriptionResults SubscriptionManager::get_lwc_metrics(
    const Tags* common_tags, int64_t frequency) const noexcept {
  using interpreter::TagsValuePair;

  SubscriptionResults result;
  // get all the subscriptions for a given interval (ignoring main)
  auto subs = subs_for_frequency(frequency);

  // get all the measurements that will be used
  // TODO(dmuino): deal with multiple frequencies
  auto measurements = registry_->measurements();

  // gather all metrics generated by our subscriptions
  TagsValuePairs tagsValuePairs;
  std::transform(measurements.begin(), measurements.end(),
                 std::back_inserter(tagsValuePairs),
                 [common_tags](const Measurement& m) {
                   return TagsValuePair::from(m, common_tags);
                 });

  return generate_sub_results(evaluator_, subs, tagsValuePairs);
}

// note the lifetime of common_tags is important here. For performance reasons
// we just store a pointer in the Ids, and that pointer needs to be valid during
// the whole send process (get measurements, get tagsvaluepairs, eval, send)
TagsValuePairs get_main_measurements(const util::Config& cfg,
                                     const Tags* common_tags,
                                     Registry* registry,
                                     const Evaluator& evaluator) {
  using interpreter::Query;
  using interpreter::TagsValuePair;

  const auto all_measurements = registry->measurements();
  const auto freq_value = util::secs_for_millis(kMainFrequencyMillis);
  Tags freq_tags{{"id", freq_value.c_str()}};
  registry->gauge("atlas.client.rawMeasurements", freq_tags)
      ->Update(all_measurements.size());
  auto result = TagsValuePairs();
  auto logger = Logger();

  if (all_measurements.empty()) {
    logger->info("No metrics registered.");
    return result;
  }

  // apply the rollup config
  const auto& rules = cfg.PublishConfig();
  if (rules.empty()) {
    logger->info("No publish configuration. Assuming :all for {} measurements.",
                 all_measurements.size());
    std::transform(all_measurements.begin(), all_measurements.end(),
                   std::back_inserter(result),
                   [common_tags](const Measurement& m) {
                     return TagsValuePair::from(m, common_tags);
                   });
    return result;
  }
  log_rules(rules, all_measurements.size());

  // metrics that match each rule
  auto measurements_for_rule = std::vector<TagsValuePairs>(rules.size());

  for (const auto& m : all_measurements) {
    auto tagsValue = TagsValuePair::from(m, common_tags);
    for (auto i = 0u; i < rules.size(); ++i) {
      const auto& query = evaluator.get_query(rules[i]);

      if (query->Matches(*tagsValue)) {
        measurements_for_rule[i].emplace_back(std::move(tagsValue));
        break;
      }
    }
    // drop if we don't match
  }

  // measurements for each rule have been collected
  // now apply the rules
  auto i = 0;
  for (const auto& rule : rules) {
    // create a dummy subscription to re-use the evaluation engine in the
    // subscription_registry
    log_measures_for_rule(rule, measurements_for_rule[i]);
    auto rule_result = evaluator.eval(rule, measurements_for_rule[i]);
    ++i;
    // add all resulting measurements to our overall result
    std::move(rule_result.begin(), rule_result.end(),
              std::back_inserter(result));
  }

  registry->gauge("atlas.client.mainMeasurements", freq_tags)
      ->Update(result.size());
  return result;
}

void SubscriptionManager::send_to_main() {
  static auto send_timer = registry_->timer("atlas.client.sendToMain");
  const auto& clock = registry_->clock();
  auto start = clock.MonotonicTime();

  interpreter::Interpreter interpreter{
      std::make_unique<interpreter::ClientVocabulary>()};
  auto config = config_manager_.GetConfig();
  auto common_tags = config->CommonTags();
  auto metrics =
      get_main_measurements(*config, &common_tags, registry_.get(), evaluator_);

  // all our metrics are normalized, so send them with a timestamp at the start
  // of the step
  auto timestamp =
      clock.WallTime() / kMainFrequencyMillis * kMainFrequencyMillis;
  PushMeasurements(timestamp, metrics);
  auto nanos = clock.MonotonicTime() - start;
  auto millis = nanos / 1e6;
  Logger()->info("Sent {} metrics to {} in {}ms", metrics.size(),
                 config->EndpointConfiguration().publish, millis);
  send_timer->Record(nanos);
}

void SubscriptionManager::PushMeasurements(
    int64_t now_millis, const TagsValuePairs& measurements) const {
  auto config = config_manager_.GetConfig();
  publisher_.PushMeasurements(*config, now_millis, measurements);
}

}  // namespace meter
}  // namespace atlas
