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

using interpreter::Evaluator;
using interpreter::TagsValuePairs;
using util::kFastestFrequencyMillis;
using util::kMainFrequencyMillis;
using util::kMainMultiple;
using util::Logger;

using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::system_clock;

SubscriptionManager::SubscriptionManager(
    SystemClockWithOffset* clock,
    const util::ConfigManager& config_manager) noexcept
    : clock_(clock),
      config_manager_(config_manager),
      registry_(
          std::make_shared<AtlasRegistry>(kFastestFrequencyMillis, clock_)),
      main_registry_(kFastestFrequencyMillis, kMainFrequencyMillis),
      publisher_(registry_) {
  for (auto i = 0; i < kMainMultiple; ++i) {
    sub_registries_[i] = std::make_unique<ConsolidationRegistry>(
        kFastestFrequencyMillis, kFastestFrequencyMillis * (i + 1));
  }
}

static constexpr int64_t kMinWaitMillis = 1;

void SubscriptionManager::update_registries() noexcept {
  const auto& config = config_manager_.GetConfig();

  auto measurements = registry_->measurements();
  auto logger = Logger();
  if (config->IsMainEnabled()) {
    logger->debug("updating main registry from {} measurements",
                  measurements.size());
    main_registry_.update_from(measurements);
  }

  if (config->AreSubsEnabled()) {
    // update all freq intervals that have subscriptions
    for (auto i = 0; i < kMainMultiple; ++i) {
      if (!subscriptions_[i].empty()) {
        logger->debug("updating values for subs with freq={}",
                      (i + 1) * kFastestFrequencyMillis);
        sub_registries_[i]->update_from(measurements);
      }
    }
  }
}

void SubscriptionManager::main_sender() noexcept {
  int64_t run = 0;
  Logger()->info("Starting main sender thread");
  while (should_run_) {
    auto logger = Logger();
    ++run;
    auto start = system_clock::now();
    update_metrics();
    update_registries();

    auto config = config_manager_.GetConfig();
    auto idx = run % kMainMultiple;
    if (idx == (kMainMultiple - 1) && config->IsMainEnabled()) {
      logger->debug("Sending metrics to publish using the main registry");
      try {
        send_to_main();
      } catch (const std::exception& e) {
        logger->error("Error sending to main publish cluster: {}", e.what());
      }
    } else if (idx == kMainMultiple - 1) {
      logger->info(
          "Not sending anything to the main publish cluster (disabled)");
    }

    if (config->AreSubsEnabled()) {
      // send sub results if needed
      if (!subscriptions_[idx].empty()) {
        auto sub_millis = (idx + 1) * kFastestFrequencyMillis;
        logger->debug("sending subscriptions results for {}ms", sub_millis);
        try {
          send_subscriptions(sub_millis);
        } catch (const std::exception& e) {
          logger->error("Error sending subscriptions for {}s {}",
                        sub_millis / 1000, e.what());
        }
      }
    } else {
      logger->debug("subscriptions are disabled.");
    }
    auto elapsed_millis =
        duration_cast<milliseconds>(system_clock::now() - start).count();
    auto wait_millis =
        std::max(kMinWaitMillis, kFastestFrequencyMillis - elapsed_millis);
    logger->debug(
        "Waiting {} millis until next main sender/updater run ({}/{})",
        wait_millis, idx, kMainMultiple - 1);
    std::unique_lock<std::mutex> lock{mutex};
    cv.wait_for(lock, milliseconds(wait_millis));
  }
}

void SubscriptionManager::sub_refresher() noexcept {
  Logger()->debug("Starting sub_refresher thread");
  while (should_run_) {
    auto logger = Logger();
    try {
      auto start = system_clock::now();
      const auto& config = config_manager_.GetConfig();
      const auto& endpoints = config->EndpointConfiguration();
      auto subs_endpoint = endpoints.subscriptions;
      if (config->AreSubsEnabled()) {
        logger->info("Refreshing subscriptions from {}", subs_endpoint);
        refresh_subscriptions(subs_endpoint);
      }
      auto end = system_clock::now();
      auto elapsed_millis = duration_cast<milliseconds>(end - start).count();
      auto refresh_millis = config->SubRefreshMillis();
      auto wait_millis =
          std::max(kMinWaitMillis, refresh_millis - elapsed_millis);
      logger->debug("Waiting {}ms for next refresh subscription cycle",
                    wait_millis);
      std::unique_lock<std::mutex> lock{mutex};
      cv.wait_for(lock, milliseconds(wait_millis));
    } catch (std::exception& e) {
      logger->info("Ignoring exception while refreshing configs: {}", e.what());
    }
  }
}

ParsedSubscriptions ParseSubscriptions(char* subs_buffer) {
  using rapidjson::Document;

  Document document;
  document.ParseInsitu(subs_buffer);
  ParsedSubscriptions subs;
  auto logger = Logger();
  if (document.IsObject()) {
    auto expressions = document["expressions"].GetArray();
    for (auto& e : expressions) {
      if (e.IsObject()) {
        auto expr = e.GetObject();
        auto freq = expr["frequency"].GetInt64();
        auto multiple = freq / kFastestFrequencyMillis;
        if (multiple >= 1 && multiple <= kMainMultiple) {
          subs[multiple - 1].emplace_back(Subscription{
              expr["id"].GetString(), freq, expr["expression"].GetString()});
        } else {
          logger->warn("Ignoring id={} expr={} freq={}", expr["id"].GetString(),
                       expr["expression"].GetString(), freq);
        }
      }
    }
  }
  if (logger->should_log(spdlog::level::debug)) {
    for (auto i = 0u; i < kMainMultiple; ++i) {
      const auto& s = subs[i];
      if (!s.empty()) {
        logger->debug("Number of subs for {}ms: {}",
                      (i + 1) * kFastestFrequencyMillis, s.size());
        logger->trace("Subs for {}ms: {}", (i + 1) * kFastestFrequencyMillis,
                      s);
      }
    }
  }

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
      auto subscriptions =
          ParseSubscriptions(const_cast<char*>(subs_str.c_str()));
      std::lock_guard<std::mutex> guard{subscriptions_mutex};
      subscriptions_ = std::move(subscriptions);
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

void SubscriptionManager::flush_metrics() noexcept {
  auto logger = Logger();

  logger->debug("Updating registries before flushing");
  // update registries with the partial set of metrics from the 5s registry
  update_registries();

  // send all
  auto config = config_manager_.GetConfig();
  if (config->IsMainEnabled()) {
    try {
      logger->info("Sending to main publish cluster");
      send_to_main();
    } catch (const std::exception& e) {
      logger->error("Error sending to main publish cluster: {}", e.what());
    }
  }
  if (config->AreSubsEnabled()) {
    for (auto i = 0; i < kMainMultiple; ++i) {
      if (!subscriptions_[i].empty()) {
        auto millis = (i + 1) * kFastestFrequencyMillis;
        try {
          logger->info("Sending subs for {}s", millis / 1000);
          send_subscriptions(millis);
        } catch (const std::exception& e) {
          logger->error("Error sending subscriptions for {}s: {}",
                        millis / 1000, e.what());
        }
      }
    }
  }
}

void SubscriptionManager::Stop() noexcept {
  if (should_run_) {
    should_run_ = false;
    cv.notify_all();
    Logger()->debug("Stopping main sender");
    sender_thread.join();
    Logger()->debug("Stopping sub_refresher");
    sub_refresher_thread.join();

    flush_metrics();
    Logger()->info("Advancing clock and flushing metrics");
    clock_->SetOffset(4990);
    flush_metrics();
  }
}

SubscriptionManager::~SubscriptionManager() {
  if (should_run_) {
    Stop();
  }
}

void SubscriptionManager::Start() noexcept {
  Logger()->debug("Starting subscription manager");
  should_run_ = true;
  sub_refresher_thread = std::thread(&SubscriptionManager::sub_refresher, this);
  sender_thread = std::thread(&SubscriptionManager::main_sender, this);
}

Subscriptions SubscriptionManager::subs_for_frequency(size_t sub_idx) const
    noexcept {
  std::lock_guard<std::mutex> guard{subscriptions_mutex};
  assert(sub_idx < kMainMultiple);
  return subscriptions_[sub_idx];
}

void SubscriptionManager::send_subscriptions(int64_t millis) noexcept {
  using util::secs_for_millis;

  const auto& clock = registry_->clock();
  const auto start = clock.MonotonicTime();
  auto timer = registry_->timer("atlas.client.sendLwc",
                                Tags{{"id", secs_for_millis(millis).c_str()}});
  auto cfg = config_manager_.GetConfig();
  auto common_tags = cfg->CommonTags();

  auto sub_idx = static_cast<size_t>((millis / kFastestFrequencyMillis) - 1);
  const auto& sub_results = get_lwc_metrics(&common_tags, sub_idx);
  if (sub_results.empty()) {
    Logger()->debug("No subscription results found");
  } else {
    Logger()->debug("Sending {} subscription results", sub_results.size());
    publisher_.SendSubscriptions(*cfg, millis, sub_results);
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
    os << "Applying [";
    bool first = true;
    for (const auto& rule : rules) {
      if (!first) {
        os << ", ";
      } else {
        first = false;
      }
      os << rule;
    }
    os << "] to " << num_measurements << " measurements";
    logger->debug(os.str());
  }
}

static void log_measures_for_rule(const std::string& rule,
                                  const interpreter::TagsValuePairs& pairs) {
  auto logger = Logger();
  if (logger->should_log(spdlog::level::debug)) {
    logger->debug("Rule: [{} - {} measurements]", rule, pairs.size());
    if (logger->should_log(spdlog::level::trace)) {
      for (const auto& pair : pairs) {
        logger->trace("\t{}", *pair);
      }
    }
  }
}

SubscriptionResults generate_sub_results(const Evaluator& evaluator,
                                         const Subscriptions& subs,
                                         const TagsValuePairs& pairs) {
  using interpreter::TagsValuePair;
  SubscriptionResults result;

  Logger()->info("Generating sub results for {} subscriptions", subs.size());
  auto i = 0;
  for (auto& s : subs) {
    i++;
    if (i % 1000 == 0) {
      Logger()->debug("Processed {} subs", i);
    }
    auto expr_result = evaluator.eval(s.expression, pairs);
    for (const auto& pair : expr_result) {
      auto value = pair->value();
      if (!std::isnan(value)) {
        result.emplace_back(SubscriptionMetric{s.id, pair->all_tags(), value});
      }
    }
  }
  Logger()->info(
      "Done generating sub results for {} subscriptions. Number of results = "
      "{}",
      subs.size(), result.size());
  return result;
}

SubscriptionResults SubscriptionManager::get_lwc_metrics(
    const Tags* common_tags, size_t sub_idx) const noexcept {
  using interpreter::TagsValuePair;

  SubscriptionResults result;
  // get all the subscriptions for a given interval
  auto subs = subs_for_frequency(sub_idx);

  Logger()->debug("Subs[{}] idx: {}", sub_idx, subs.size());

  // get all the measurements that will be used
  auto measurements = sub_registries_[sub_idx]->measurements();
  Logger()->debug("Measurements for subs[{}] idx: {}", sub_idx,
                  measurements.size());

  // gather all metrics generated by our subscriptions
  TagsValuePairs tagsValuePairs;
  tagsValuePairs.reserve(measurements.size());
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
                                     const Measurements& all_measurements,
                                     Registry* registry,
                                     const Evaluator& evaluator) {
  using interpreter::Query;
  using interpreter::TagsValuePair;

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
  auto metrics = get_main_measurements(*config, &common_tags,
                                       main_registry_.measurements(),
                                       registry_.get(), evaluator_);

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
