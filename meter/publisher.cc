#include "publisher.h"
#include "validation.h"
#include "../util/config.h"
#include "../util/gzip.h"
#include "../util/http.h"
#include "../util/intern.h"
#include "../util/json.h"
#include "../util/logger.h"
#include <chrono>

namespace atlas {
namespace meter {
using util::Config;
using util::intern_str;
using util::Logger;

using interpreter::TagsValuePairs;
using meter::SubscriptionResults;
using std::chrono::system_clock;

const static Tags atlas_client_tags{
    {intern_str("class"), intern_str("NetflixAtlasObserver")},
    {intern_str("id"), intern_str("main-vip")}};

inline void UpdateSentStat(Registry* registry, int64_t delta) {
  registry->counter("numMetricsSent", atlas_client_tags)->Add(delta);
}

inline void UpdateHttpErrorsStat(Registry* registry, int status_code,
                                 int64_t errors) {
  auto tags = atlas_client_tags;
  tags.add("error", "httpError");
  tags.add("statusCode", std::to_string(status_code).c_str());

  registry->counter("numMetricsDropped", tags)->Add(errors);
}

inline void UpdateTotalSentStat(Registry* registry, int64_t total_sent) {
  registry->counter("numMetricsTotal", atlas_client_tags)->Add(total_sent);
}

inline void UpdateValidationErrorStats(Registry* registry, int64_t errors) {
  auto tags = atlas_client_tags;
  tags.add("error", "validationFailed");
  registry->counter("numMetricsDropped", tags)->Add(errors);
}

static void DumpJson(const std::string& dir, const std::string& base_file_name,
                     const rapidjson::Document& payload) {
  auto millis = system_clock::now().time_since_epoch().count();
  auto logger = Logger();
  auto file_name = fmt::format("{}/{}{}.json.gz", dir, base_file_name, millis);

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

static void SendBatch(Registry* registry, const util::http& client,
                      const util::Config& config, int64_t now_millis,
                      const interpreter::TagsValuePairs::const_iterator& first,
                      const interpreter::TagsValuePairs::const_iterator& last) {
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
    UpdateValidationErrorStats(registry, num_metrics - added);
  }
  UpdateTotalSentStat(registry, num_metrics);
  if (added == 0) {
    return;
  }

  auto start = registry->clock().MonotonicTime();
  auto http_res = client.post(endpoints.publish, payload);
  registry->timer("atlas.client.mainBatch")
      ->Record(registry->clock().MonotonicTime() - start);
  if (log_config.dump_metrics) {
    DumpJson("/tmp", "main_batch_", payload);
  }
  if (http_res != 200) {
    logger->error("Unable to send batch of {} measurements to publish: {}",
                  num_measurements, http_res);

    UpdateHttpErrorsStat(registry, http_res, added);
  } else {
    UpdateSentStat(registry, added);
  }
}

static void PushInParallel(std::shared_ptr<Registry> registry,
                           const util::Config& config, int64_t now_millis,
                           const interpreter::TagsValuePairs& measurements) {
  static auto timer = registry->timer("atlas.client.parallelPost");

  const auto& http_cfg = config.HttpConfiguration();
  util::http client{registry, http_cfg};

  const auto& endpoints = config.EndpointConfiguration();
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

  auto start = registry->clock().MonotonicTime();
  auto http_codes = client.post_batches(endpoints.publish, batches);
  UpdateValidationErrorStats(registry.get(), validation_errors);
  UpdateTotalSentStat(registry.get(),
                      static_cast<int64_t>(measurements.size()));
  for (auto i = 0u; i < http_codes.size(); ++i) {
    auto http_code = http_codes[i];
    auto num_measurements = batch_sizes[i];
    if (http_code != 200) {
      Logger()->error("Unable to send batch of {} measurements to publish: {}",
                      num_measurements, http_code);

      UpdateHttpErrorsStat(registry.get(), http_code, num_measurements);
    } else {
      UpdateSentStat(registry.get(), num_measurements);
    }
  }

  timer->Record(registry->clock().MonotonicTime() - start);
}

static void PushSerially(std::shared_ptr<Registry> registry,
                         const util::Config& config, int64_t now_millis,
                         const interpreter::TagsValuePairs& measurements) {
  const auto& http_cfg = config.HttpConfiguration();
  util::http client{registry, http_cfg};
  auto batch_size =
      static_cast<TagsValuePairs::difference_type>(http_cfg.batch_size);

  auto from = measurements.begin();
  auto end = measurements.end();
  while (from != end) {
    auto to_end = std::distance(from, end);
    auto to_advance = std::min(batch_size, to_end);
    auto to = from;
    std::advance(to, to_advance);
    SendBatch(registry.get(), client, config, now_millis, from, to);
    from = to;
  }
}

static Tags with_freq(int64_t freq_millis) {
  Tags result;
  auto freq_value = util::secs_for_millis(freq_millis);
  result.add("id", freq_value.c_str());
  return result;
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

void Publisher::batch_to_lwc(
    const util::http& client, const Config& config, int64_t freq_millis,
    const SubscriptionResults::const_iterator& first,
    const SubscriptionResults::const_iterator& last) const noexcept {
  auto tags = with_freq(freq_millis);
  auto timer = registry_->timer("atlas.client.lwcBatch", tags);
  auto errors = registry_->counter("atlas.client.sendLwcErrors", tags);
  const auto& clock = registry_->clock();
  auto start = clock.MonotonicTime();

  auto metrics = SubResultsToJson(clock.WallTime(), first, last);
  if (config.LogConfiguration().dump_subscriptions) {
    auto file_name = fmt::format("lwc_{:02}s_", freq_millis / 1000);
    DumpJson("/tmp", file_name, metrics);
  }
  const auto& endpoints = config.EndpointConfiguration();
  auto res = client.post(endpoints.evaluate, metrics);
  if (res != 200) {
    Logger()->error("Failed to POST to {}: {}", endpoints.evaluate, res);
    errors->Increment();
  }
  timer->Record(clock.MonotonicTime() - start);
}

void Publisher::PushMeasurements(const Config& config, int64_t now_millis,
                                 const TagsValuePairs& measurements) const
    noexcept {
  if (config.HttpConfiguration().send_in_parallel) {
    PushInParallel(registry_, config, now_millis, measurements);
  } else {
    PushSerially(registry_, config, now_millis, measurements);
  }
}

void Publisher::SendSubscriptions(const Config& config,
                                  const SubscriptionResults& sub_results) const
    noexcept {
  const auto& http_cfg = config.HttpConfiguration();
  auto batch_size =
      static_cast<SubscriptionResults::difference_type>(http_cfg.batch_size);
  auto from = sub_results.begin();
  auto end = sub_results.end();
  util::http client{registry_, http_cfg};

  while (from != end) {
    auto to_end = std::distance(from, end);
    auto to_advance = std::min(batch_size, to_end);
    auto to = from;
    std::advance(to, to_advance);
    batch_to_lwc(client, config, util::kMainFrequencyMillis, from, to);
    from = to;
  }
}

}  // namespace meter
}  // namespace atlas
