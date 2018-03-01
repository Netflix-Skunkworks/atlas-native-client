#include "subscription_manager.h"
#include "../atlas_client.h"
#include "../util/http.h"
#include "../util/json.h"
#include "../util/logger.h"
#include "../util/string_pool.h"
#include "../util/strings.h"
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

SubscriptionManager::SubscriptionManager(
    const util::ConfigManager& config_manager, SubscriptionRegistry& registry)
    : config_manager_(config_manager), registry_(registry) {}

using interpreter::TagsValuePairs;
using util::Logger;
using util::intern_str;
using util::kMainFrequencyMillis;

using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::system_clock;

static constexpr int64_t kMinWaitMillis = 1;

void SubscriptionManager::MainSender(
    std::chrono::seconds initial_delay) noexcept {
  Logger()->info("Waiting for {} seconds to send the first batch to main.",
                 initial_delay.count());
  std::this_thread::sleep_for(initial_delay);
  while (should_run_) {
    auto start = system_clock::now();
    const auto& config = config_manager_.GetConfig();
    if (config->IsMainEnabled()) {
      Logger()->debug("SendToMain()");
      try {
        UpdateMetrics();
        SendToMain();
      } catch (const std::exception& e) {
        Logger()->error("Error sending to main publish cluster: {}", e.what());
      }
    } else {
      Logger()->info(
          "Not sending anything to the main publish cluster (disabled)");
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

static void updateAlertServer(const std::string& endpoint,
                              const util::HttpConfig& config) {
  util::http client(config);
  auto res =
      client.post(endpoint, "Content-type: application/octet-stream", "", 0);
  Logger()->debug("Got {} from alert server {}", res, endpoint);
}

void SubscriptionManager::SubRefresher() noexcept {
  static uint64_t refresher_runs = 0;

  while (should_run_) {
    try {
      auto start = system_clock::now();
      const auto& config = config_manager_.GetConfig();
      const auto& endpoints = config->EndpointConfiguration();
      auto subs_endpoint = endpoints.subscriptions;
      if (config->AreSubsEnabled()) {
        Logger()->info("Refreshing subscriptions from {}", subs_endpoint);
        RefreshSubscriptions(subs_endpoint);
      }
      // HACK (temporary until lwc does alerts)
      // notify alert server that we do not support on-instance alerts
      if (config->ShouldNotifyAlertServer() && refresher_runs % 30 == 0) {
        Logger()->debug(
            "Notifying alert server about our lack of on-instance alerts "
            "support {}",
            endpoints.check_cluster);
        updateAlertServer(endpoints.check_cluster, config->HttpConfiguration());
      }
      ++refresher_runs;
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

void SubscriptionManager::SubSender(int64_t millis) noexcept {
  while (should_run_) {
    auto start = system_clock::now();
    try {
      SendMetricsForInterval(millis);
    } catch (std::exception& e) {
      Logger()->info("Ignoring exception while sending metrics for {}ms: {}",
                     millis, e.what());
    }
    auto end = system_clock::now();
    auto elapsed_millis = duration_cast<milliseconds>(end - start).count();
    auto delta = millis - elapsed_millis;
    auto wait_millis = delta > 0 ? delta : 0;
    Logger()->info("Waiting {}ms for next send cycle for {}ms interval",
                   wait_millis, millis);
    std::unique_lock<std::mutex> lock{mutex};
    cv.wait_for(lock, milliseconds(wait_millis));
  }
}

// non-static for testing
rapidjson::Document SubResultsToJson(
    int64_t now_millis, const SubscriptionResults::const_iterator& first,
    const SubscriptionResults::const_iterator& last) {
  using rapidjson::Document;
  using rapidjson::SizeType;
  using rapidjson::Value;
  using rapidjson::kArrayType;
  using rapidjson::kObjectType;

  Document ret{kObjectType};
  auto& alloc = ret.GetAllocator();
  ret.AddMember("timestamp", now_millis, alloc);
  Value metrics{kArrayType};

  for (auto it = first; it != last; ++it) {
    const auto& subscriptionResult = *it;
    Value sub{kObjectType};
    const auto& id = subscriptionResult.id;
    sub.AddMember("id",
                  Value().SetString(id.c_str(),
                                    static_cast<SizeType>(id.length()), alloc),
                  alloc);
    Value tags{kObjectType};
    for (const auto& kv : subscriptionResult.tags) {
      const auto* key = kv.first.get();
      Value k(key, static_cast<SizeType>(strlen(key)), alloc);

      const auto* val = kv.second.get();
      Value v(val, static_cast<SizeType>(strlen(val)), alloc);
      tags.AddMember(k, v, alloc);
    }
    sub.AddMember("tags", tags, alloc);
    sub.AddMember("value", subscriptionResult.value, alloc);
    metrics.PushBack(sub, alloc);
  }
  ret.AddMember("metrics", metrics, alloc);
  return ret;
}

// Get the set of intervals in milliseconds from a list of subscriptions
std::set<int64_t> GetIntervals(const Subscriptions* subs) {
  std::set<int64_t> res;
  if (subs != nullptr) {
    std::transform(subs->begin(), subs->end(), std::inserter(res, res.begin()),
                   [](const Subscription& s) { return s.frequency; });
  }
  return res;
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
        subs->push_back(Subscription{expr["id"].GetString(),
                                     expr["frequency"].GetInt64(),
                                     expr["expression"].GetString()});
      }
    }
  }

  return subs;
}
void SubscriptionManager::RefreshSubscriptions(
    const std::string& subs_endpoint) {
  static auto timer_refresh_subs =
      atlas_registry.timer("atlas.client.refreshSubs");
  static auto refresh_parsing_errors =
      atlas_registry.counter(atlas_registry.CreateId(
          "atlas.client.refreshSubsErrors", Tags{{"error", "json"}}));
  static auto refresh_http_errors =
      atlas_registry.counter(atlas_registry.CreateId(
          "atlas.client.refreshSubsErrors", Tags{{"error", "http"}}));
  std::string subs_str;
  const auto& logger = Logger();

  auto cfg = config_manager_.GetConfig();
  util::http http_client(cfg->HttpConfiguration());
  auto start = atlas_registry.clock().MonotonicTime();
  auto http_res =
      http_client.conditional_get(subs_endpoint, current_etag, &subs_str);
  timer_refresh_subs->Record(atlas_registry.clock().MonotonicTime() - start);
  if (http_res == 200) {
    auto subscriptions = ParseSubscriptions(subs_str);
    auto old_subs = subscriptions_.exchange(subscriptions);
    try {
      const auto subs = subscriptions_.load(std::memory_order_relaxed);
      registry_.update_subscriptions(subs);
      auto new_intervals = GetIntervals(subs);
      for (auto i : new_intervals) {
        if (sender_intervals_.find(i) == sender_intervals_.end()) {
          logger->info("New sender for {} milliseconds detected. Scheduling",
                       i);
          sub_senders.emplace_back(&SubscriptionManager::SubSender, this, i);
        }
      }
    } catch (...) {
      delete old_subs;
      refresh_parsing_errors->Increment();
    }
  } else if (http_res == 304) {
    logger->debug("Not refreshing subscriptions. Not modified");
  } else {
    logger->error("Failed to refresh subscriptions: {}", http_res);
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
      SendToMain();
    }
    for (auto& t : sub_senders) {
      t.join();
    }
    main_sender_thread.join();
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
  sub_refresher_thread = std::thread(&SubscriptionManager::SubRefresher, this);
  auto initialDelay = GetInitialDelay();
  main_sender_thread =
      std::thread(&SubscriptionManager::MainSender, this, initialDelay);
}

static void DumpJson(const std::string& dir, const std::string& base_file_name,
                     const rapidjson::Document& payload) {
  auto millis = system_clock::now().time_since_epoch().count();
  auto logger = Logger();
  std::ostringstream os;
  os << dir << "/" << base_file_name << millis << ".json.gz";
  auto file_name = os.str();

  // get a c string
  rapidjson::StringBuffer buffer;
  auto c_str = util::JsonGetString(buffer, payload);

  auto fd = gzopen(file_name.c_str(), "wb");
  if (fd != nullptr) {
    gzbuffer(fd, 65536);
    auto written = gzputs(fd, c_str);
    if (written <= 0) {
      logger->error("Unable to write compressed file {}", file_name);
    }
    gzclose(fd);
  } else {
    logger->error("Unable to open {}", file_name);
  }
}

static void SendBatchToLwc(const util::http& client, const util::Config& config,
                           int64_t freq_millis,
                           const SubscriptionResults::const_iterator& first,
                           const SubscriptionResults::const_iterator& last) {
  static auto sendBatchLwcId =
      atlas_registry.CreateId("atlas.client.lwcBatch", kEmptyTags);
  static auto errorId =
      atlas_registry.CreateId("atlas.client.sendLwcErrors", kEmptyTags);
  const auto& clock = atlas_registry.clock();
  auto start = clock.MonotonicTime();
  const auto& msecs_str = std::to_string(freq_millis);
  Tag freq_tag = Tag::of("freq", msecs_str);
  auto timer = atlas_registry.timer(sendBatchLwcId->WithTag(freq_tag));

  auto metrics = SubResultsToJson(clock.WallTime(), first, last);
  if (config.LogConfiguration().dump_subscriptions) {
    std::string file_name{"lwc_"};
    DumpJson("/tmp", file_name + msecs_str + "_", metrics);
  }
  const auto& endpoints = config.EndpointConfiguration();
  auto res = client.post(endpoints.evaluate, metrics);
  if (res != 200) {
    Logger()->error("Failed to POST to {}: {}", endpoints.evaluate, res);
    atlas_registry.counter(errorId->WithTag(freq_tag))->Increment();
  }
  timer->Record(clock.MonotonicTime() - start);
}

void SubscriptionManager::SendMetricsForInterval(int64_t millis) noexcept {
  static auto sendId =
      atlas_registry.CreateId("atlas.client.sendLwc", kEmptyTags);

  const auto& clock = atlas_registry.clock();
  const auto start = clock.MonotonicTime();
  const auto& msecs_str = std::to_string(millis);
  const Tag freq_tag = Tag::of("freq", msecs_str);

  auto timer = atlas_registry.timer(sendId->WithTag(freq_tag));
  auto cfg = config_manager_.GetConfig();
  const auto& sub_results = registry_.GetLwcMetricsForInterval(*cfg, millis);
  const auto& http_cfg = cfg->HttpConfiguration();
  util::http http_client(http_cfg);

  auto batch_size =
      static_cast<SubscriptionResults::difference_type>(http_cfg.batch_size);
  auto from = sub_results.begin();
  auto end = sub_results.end();
  while (from != end) {
    auto to_end = std::distance(from, end);
    auto to_advance = std::min(batch_size, to_end);
    auto to = from;
    std::advance(to, to_advance);
    SendBatchToLwc(http_client, *cfg, millis, from, to);
    from = to;
  }
  timer->Record(clock.MonotonicTime() - start);
}

// not static for testing
rapidjson::Document MeasurementsToJson(
    int64_t now_millis,
    const interpreter::TagsValuePairs::const_iterator& first,
    const interpreter::TagsValuePairs::const_iterator& last, bool validate,
    int64_t* metrics_added) {
  using rapidjson::Document;
  using rapidjson::SizeType;
  using rapidjson::Value;
  using rapidjson::kArrayType;
  using rapidjson::kObjectType;

  int64_t added = 0;
  Document payload{kObjectType};
  auto& alloc = payload.GetAllocator();

  Value common_tags_section{kObjectType};

  Value json_metrics{kArrayType};
  for (auto it = first; it < last; it++) {
    const auto& measure = *it;
    if (std::isnan(measure->value())) {
      continue;
    }
    auto tags = measure->all_tags();
    if (validate && !validation::IsValid(tags)) {
      continue;
    }
    Value json_metric{kObjectType};
    Value json_metric_tags{kObjectType};

    for (const auto& tag : tags) {
      const char* k_str = util::ToValidCharset(tag.first).get();
      const char* v_str = util::EncodeValueForKey(tag.second, tag.first).get();

      Value k(k_str, static_cast<SizeType>(strlen(k_str)), alloc);
      Value v(v_str, static_cast<SizeType>(strlen(v_str)), alloc);
      json_metric_tags.AddMember(k, v, alloc);
    }
    ++added;

    json_metric.AddMember("tags", json_metric_tags, alloc);
    json_metric.AddMember("start", now_millis, alloc);
    json_metric.AddMember("value", measure->value(), alloc);
    json_metrics.PushBack(json_metric, alloc);
  }

  payload.AddMember("tags", common_tags_section, alloc);
  payload.AddMember("metrics", json_metrics, alloc);

  *metrics_added = added;
  return payload;
}

const static Tags atlas_client_tags{
    {intern_str("class"), intern_str("NetflixAtlasObserver")},
    {intern_str("id"), intern_str("main-vip")}};

static void UpdateSentStat(int64_t delta) {
  static auto sent = atlas_registry.counter(
      atlas_registry.CreateId("numMetricsSent", atlas_client_tags));
  sent->Add(delta);
}

static void UpdateHttpErrorsStat(int status_code, int64_t errors) {
  static auto httpErrorsId =
      atlas_registry.CreateId("numMetricsDropped", atlas_client_tags)
          ->WithTag(Tag::of("error", "httpError"));
  atlas_registry
      .counter(httpErrorsId->WithTag(
          Tag::of("statusCode", std::to_string(status_code))))
      ->Add(errors);
}

static void UpdateTotalSentStat(int64_t total_sent) {
  static auto total = atlas_registry.counter(
      atlas_registry.CreateId("numMetricsTotal", atlas_client_tags));
  total->Add(total_sent);
}

static void UpdateValidationErrorStats(int64_t errors) {
  static auto validationErrors = atlas_registry.counter(
      atlas_registry.CreateId("numMetricsDropped", atlas_client_tags)
          ->WithTag(Tag::of("error", "validationFailed")));

  validationErrors->Add(errors);
}

static void SendBatch(const util::http& client, const util::Config& config,
                      int64_t now_millis,
                      const interpreter::TagsValuePairs::const_iterator& first,
                      const interpreter::TagsValuePairs::const_iterator& last) {
  static auto timer = atlas_registry.timer("atlas.client.mainBatch");
  auto logger = Logger();

  auto num_measurements = std::distance(first, last);
  const auto& endpoints = config.EndpointConfiguration();
  const auto& log_config = config.LogConfiguration();
  logger->info("Sending batch of {} metrics to {}", num_measurements,
               endpoints.publish);
  int64_t added = 0;
  auto num_metrics = static_cast<int64_t>(num_measurements);
  auto payload = MeasurementsToJson(now_millis, first, last,
                                    config.ShouldValidateMetrics(), &added);
  if (added != num_metrics) {
    UpdateValidationErrorStats(num_metrics - added);
  }
  UpdateTotalSentStat(num_metrics);
  if (added == 0) {
    return;
  }

  auto start = atlas_registry.clock().MonotonicTime();
  auto http_res = client.post(endpoints.publish, payload);
  timer->Record(atlas_registry.clock().MonotonicTime() - start);
  if (log_config.dump_metrics) {
    DumpJson("/tmp", "main_batch_", payload);
  }
  if (http_res != 200) {
    logger->error("Unable to send batch of {} measurements to publish: {}",
                  num_measurements, http_res);

    UpdateHttpErrorsStat(http_res, added);
  } else {
    UpdateSentStat(added);
  }
}

static void PushInParallel(const util::Config& config, int64_t now_millis,
                           const interpreter::TagsValuePairs& measurements) {
  static auto timer = atlas_registry.timer("atlas.client.parallelPost");

  const auto& http_cfg = config.HttpConfiguration();
  const auto& endpoints = config.EndpointConfiguration();
  util::http client(http_cfg);
  auto batch_size =
      static_cast<TagsValuePairs::difference_type>(http_cfg.batch_size);
  std::vector<rapidjson::Document> batches;
  std::vector<int64_t> batch_sizes;
  auto from = measurements.begin();
  auto end = measurements.end();
  int64_t total_valid_metrics = 0;
  int64_t validation_errors = 0;
  while (from != end) {
    auto to_end = std::distance(from, end);
    auto to_advance = std::min(batch_size, to_end);
    auto to = from;
    std::advance(to, to_advance);

    int64_t added;
    batches.emplace_back(MeasurementsToJson(
        now_millis, from, to, config.ShouldValidateMetrics(), &added));
    if (config.LogConfiguration().dump_metrics) {
      DumpJson("/tmp", "main_batch_", batches.back());
    }
    total_valid_metrics += added;
    validation_errors += to_advance - added;
    batch_sizes.push_back(added);
    from = to;
  }

  auto start = atlas_registry.clock().MonotonicTime();
  auto http_codes = client.post_batches(endpoints.publish, batches);
  UpdateValidationErrorStats(validation_errors);
  UpdateTotalSentStat(static_cast<int64_t>(measurements.size()));
  for (auto i = 0u; i < http_codes.size(); ++i) {
    auto http_code = http_codes[i];
    auto num_measurements = batch_sizes[i];
    if (http_code != 200) {
      Logger()->error("Unable to send batch of {} measurements to publish: {}",
                      num_measurements, http_code);

      UpdateHttpErrorsStat(http_code, num_measurements);
    } else {
      UpdateSentStat(num_measurements);
    }
  }

  timer->Record(atlas_registry.clock().MonotonicTime() - start);
}

static void PushSerially(const util::Config& config, int64_t now_millis,
                         const interpreter::TagsValuePairs& measurements) {
  const auto& http_cfg = config.HttpConfiguration();
  util::http client(http_cfg);
  auto batch_size =
      static_cast<TagsValuePairs::difference_type>(http_cfg.batch_size);

  auto from = measurements.begin();
  auto end = measurements.end();
  while (from != end) {
    auto to_end = std::distance(from, end);
    auto to_advance = std::min(batch_size, to_end);
    auto to = from;
    std::advance(to, to_advance);
    SendBatch(client, config, now_millis, from, to);
    from = to;
  }
}

void SubscriptionManager::PushMeasurements(
    int64_t now_millis, const interpreter::TagsValuePairs& measurements) const {
  auto cfg = config_manager_.GetConfig();
  if (cfg->HttpConfiguration().send_in_parallel) {
    PushInParallel(*cfg, now_millis, measurements);
  } else {
    PushSerially(*cfg, now_millis, measurements);
  }
}

void SubscriptionManager::UpdateMetrics() noexcept {
  static auto pool_size = atlas_registry.gauge("atlas.client.strPoolSize");
  static auto pool_alloc = atlas_registry.gauge("atlas.client.strPoolAlloc");

  pool_size->Update(util::the_str_pool().pool_size());
  pool_alloc->Update(util::the_str_pool().alloc_size());
}

void SubscriptionManager::SendToMain() {
  static auto send_timer = atlas_registry.timer("atlas.client.sendToMain");
  const auto& clock = atlas_registry.clock();
  auto start = clock.MonotonicTime();
  auto cfg = config_manager_.GetConfig();
  const auto& endpoints = cfg->EndpointConfiguration();
  const auto common_tags = cfg->CommonTags();
  auto metrics = registry_.GetMainMeasurements(*cfg, common_tags);
  // all our metrics are normalized, so send them with a timestamp at the start
  // of the step
  auto timestamp =
      clock.WallTime() / kMainFrequencyMillis * kMainFrequencyMillis;
  PushMeasurements(timestamp, *metrics);
  auto nanos = clock.MonotonicTime() - start;
  auto millis = nanos / 1e6;
  Logger()->info("Sent {} metrics to {} in {}ms", metrics->size(),
                 endpoints.publish, millis);
  send_timer->Record(nanos);
}
}  // namespace meter
}  // namespace atlas
