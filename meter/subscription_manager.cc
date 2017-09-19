#include "subscription_manager.h"
#include "../atlas_client.h"
#include "../util/http.h"
#include "../util/json.h"
#include "../util/logger.h"
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

using util::kMainFrequencyMillis;
using util::Logger;
using util::intern_str;

using std::chrono::system_clock;
using std::chrono::duration_cast;
using std::chrono::milliseconds;

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
    std::this_thread::sleep_for(milliseconds(wait_millis));
  }
}

static void updateAlertServer(const std::string& endpoint, int connect_timeout,
                              int read_timeout) {
  util::http client;
  auto res = client.post(endpoint, connect_timeout, read_timeout,
                         "Content-type: application/octet-stream", "", 0);
  Logger()->debug("Got {} from alert server {}", res, endpoint);
}

void SubscriptionManager::SubRefresher() noexcept {
  static uint64_t refresher_runs = 0;

  while (should_run_) {
    try {
      auto start = system_clock::now();
      const auto& config = config_manager_.GetConfig();
      auto subs_endpoint = config->SubsEndpoint();
      if (config->AreSubsEnabled()) {
        Logger()->info("Refreshing subscriptions from {}", subs_endpoint);
        RefreshSubscriptions(subs_endpoint);
      }
      // HACK (temporary until lwc does alerts)
      // notify alert server that we do not support on-instance alerts
      if (config->ShouldNotifyAlertServer() && refresher_runs % 30 == 0) {
        auto check_cluster_endpoint = config->CheckClusterEndpoint();
        Logger()->debug(
            "Notifying alert server about our lack of on-instance alerts "
            "support {}",
            check_cluster_endpoint);
        updateAlertServer(check_cluster_endpoint, config->ConnectTimeout(),
                          config->ReadTimeout());
      }
      ++refresher_runs;
      auto end = system_clock::now();
      auto elapsed_millis = duration_cast<milliseconds>(end - start).count();
      auto refresh_millis = config->SubRefreshMillis();
      auto wait_millis =
          std::max(kMinWaitMillis, refresh_millis - elapsed_millis);
      Logger()->debug("Waiting {}ms for next refresh subscription cycle",
                      wait_millis);
      std::this_thread::sleep_for(std::chrono::milliseconds(wait_millis));
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
    std::this_thread::sleep_for(milliseconds(wait_millis));
  }
}

// non-static for testing
rapidjson::Document SubResultsToJson(
    int64_t now_millis, const SubscriptionResults::const_iterator& first,
    const SubscriptionResults::const_iterator& last) {
  using rapidjson::Document;
  using rapidjson::kArrayType;
  using rapidjson::kObjectType;
  using rapidjson::SizeType;
  using rapidjson::Value;

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
  util::http http_client;
  std::string subs_str;
  auto logger = Logger();

  auto cfg = config_manager_.GetConfig();
  auto connect_timeout = cfg->ConnectTimeout();
  auto read_timeout = cfg->ReadTimeout();
  auto start = atlas_registry.clock().MonotonicTime();
  auto http_res = http_client.conditional_get(
      subs_endpoint, current_etag, connect_timeout, read_timeout, subs_str);
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
          std::thread sub_sender_thread{&SubscriptionManager::SubSender, this,
                                        i};
          sub_sender_thread.detach();
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
    if (clock != nullptr) {
      Logger()->info("Advancing clock and flushing metrics");
      clock->SetOffset(59900);
      SendToMain();
    }
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
  std::thread sub_refresher_thread(&SubscriptionManager::SubRefresher, this);
  sub_refresher_thread.detach();

  auto initialDelay = GetInitialDelay();
  std::thread main_sender_thread(&SubscriptionManager::MainSender, this,
                                 initialDelay);
  main_sender_thread.detach();
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
  if (config.ShouldDumpSubs()) {
    std::string file_name{"lwc_"};
    DumpJson("/tmp", file_name + msecs_str + "_", metrics);
  }
  auto res = client.post(config.EvalEndpoint(), config.ConnectTimeout(),
                         config.ReadTimeout(), metrics);
  if (res != 200) {
    Logger()->error("Failed to POST: {}", res);
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
  util::http http_client;

  auto batch_size =
      static_cast<SubscriptionResults::difference_type>(cfg->BatchSize());
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
  using rapidjson::kArrayType;
  using rapidjson::kObjectType;
  using rapidjson::SizeType;
  using rapidjson::Value;

  int64_t added = 0;
  Document payload{kObjectType};
  auto& alloc = payload.GetAllocator();

  Value common_tags_section{kObjectType};

  Value json_metrics{kArrayType};
  for (auto it = first; it < last; it++) {
    const auto& measure = *it;
    if (std::isnan(measure.value)) {
      continue;
    }
    if (validate && !validation::IsValid(measure.tags)) {
      continue;
    }
    Value json_metric{kObjectType};
    Value json_metric_tags{kObjectType};

    for (const auto& tag : measure.tags) {
      const char* k_str = util::ToValidCharset(tag.first).get();
      const char* v_str = util::EncodeValueForKey(tag.second, tag.first).get();

      Value k(k_str, static_cast<SizeType>(strlen(k_str)), alloc);
      Value v(v_str, static_cast<SizeType>(strlen(v_str)), alloc);
      json_metric_tags.AddMember(k, v, alloc);
    }
    ++added;

    json_metric.AddMember("tags", json_metric_tags, alloc);
    json_metric.AddMember("start", now_millis, alloc);
    json_metric.AddMember("value", measure.value, alloc);
    json_metrics.PushBack(json_metric, alloc);
  }

  payload.AddMember("tags", common_tags_section, alloc);
  payload.AddMember("metrics", json_metrics, alloc);

  *metrics_added = added;
  return payload;
}

static void SendBatch(const util::http& client, const util::Config& config,
                      int64_t now_millis,
                      const interpreter::TagsValuePairs::const_iterator& first,
                      const interpreter::TagsValuePairs::const_iterator& last) {
  static auto timer = atlas_registry.timer("atlas.client.mainBatch");
  const static Tags atlas_client_tags{
      {intern_str("class"), intern_str("NetflixAtlasObserver")},
      {intern_str("id"), intern_str("main-vip")}};
  static auto total = atlas_registry.counter(
      atlas_registry.CreateId("numMetricsTotal", atlas_client_tags));
  static auto sent = atlas_registry.counter(
      atlas_registry.CreateId("numMetricsSent", atlas_client_tags));
  static auto errorsId =
      atlas_registry.CreateId("numMetricsDropped", atlas_client_tags);
  static Tag httpErr = Tag::of("error", "httpError");
  static auto validationErrors = atlas_registry.counter(
      errorsId->WithTag(Tag::of("error", "validationFailed")));
  auto logger = Logger();

  auto num_measurements = std::distance(first, last);
  logger->info("Sending batch of {} metrics to {}", num_measurements,
               config.PublishEndpoint());
  // TODO(dmuino): retries
  int64_t added = 0;
  auto num_metrics = static_cast<int64_t>(num_measurements);
  auto payload = MeasurementsToJson(now_millis, first, last, true, &added);
  if (added != num_metrics) {
    validationErrors->Add(num_metrics - added);
  }
  total->Add(num_metrics);
  if (added == 0) {
    return;
  }

  auto start = atlas_registry.clock().MonotonicTime();
  auto http_res = client.post(config.PublishEndpoint(), config.ConnectTimeout(),
                              config.ReadTimeout(), payload);
  timer->Record(atlas_registry.clock().MonotonicTime() - start);
  if (config.ShouldDumpMetrics()) {
    DumpJson("/tmp", "main_batch_", payload);
  }
  if (http_res != 200) {
    logger->error("Unable to send batch of {} measurements to publish: {}",
                  num_measurements, http_res);

    atlas_registry
        .counter(errorsId->WithTag(httpErr)->WithTag(
            Tag::of("statusCode", std::to_string(http_res))))
        ->Add(added);
  } else {
    sent->Add(added);
  }
}

void SubscriptionManager::PushMeasurements(
    int64_t now_millis, const interpreter::TagsValuePairs& measurements) const {
  using interpreter::TagsValuePairs;

  util::http client;
  auto cfg = config_manager_.GetConfig();
  auto batch_size =
      static_cast<TagsValuePairs::difference_type>(cfg->BatchSize());

  auto from = measurements.begin();
  auto end = measurements.end();
  while (from != end) {
    auto to_end = std::distance(from, end);
    auto to_advance = std::min(batch_size, to_end);
    auto to = from;
    std::advance(to, to_advance);
    SendBatch(client, *cfg, now_millis, from, to);
    from = to;
  }
}

void SubscriptionManager::SendToMain() {
  static auto send_timer = atlas_registry.timer("atlas.client.sendToMain");
  const auto& clock = atlas_registry.clock();
  auto start = clock.MonotonicTime();
  auto cfg = config_manager_.GetConfig();
  auto metrics = registry_.GetMainMeasurements(*cfg);
  // all our metrics are normalized, so send them with a timestamp at the start
  // of the step
  auto timestamp =
      clock.WallTime() / kMainFrequencyMillis * kMainFrequencyMillis;
  PushMeasurements(timestamp, metrics);
  auto nanos = clock.MonotonicTime() - start;
  auto millis = nanos / 1e6;
  Logger()->info("Sent {} metrics to {} in {}ms", metrics.size(),
                 cfg->PublishEndpoint(), millis);
  send_timer->Record(nanos);
}
}  // namespace meter
}  // namespace atlas
