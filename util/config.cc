#include "config.h"
#include "logger.h"
#include "environment.h"
#include "../meter/id_format.h"
#include <rapidjson/document.h>

namespace atlas {
namespace util {

std::string Config::LoggingDirectory() const noexcept {
  return GetLoggingDir();
}

bool Config::IsMainEnabled() const noexcept {
  bool disabled = disabled_file_watcher_.exists();
  return !disabled && features.main;
}

bool Config::AreSubsEnabled() const noexcept {
  bool disabled = disabled_file_watcher_.exists();
  return !disabled && features.subscriptions;
}

static inline void put_if_nonempty(meter::Tags* tags, const char* key,
                                   std::string&& val) {
  if (!val.empty()) {
    tags->add(key, val.c_str());
  }
}

static meter::Tags get_default_common_tags() {
  meter::Tags common_tags_;
  put_if_nonempty(&common_tags_, "nf.node", env::instance_id());
  put_if_nonempty(&common_tags_, "nf.cluster", env::cluster());
  put_if_nonempty(&common_tags_, "nf.app", env::app());
  put_if_nonempty(&common_tags_, "nf.asg", env::asg());
  put_if_nonempty(&common_tags_, "nf.stack", env::stack());
  put_if_nonempty(&common_tags_, "nf.vmtype", env::vmtype());
  put_if_nonempty(&common_tags_, "nf.task", env::taskid());
  put_if_nonempty(&common_tags_, "nf.zone", env::zone());
  put_if_nonempty(&common_tags_, "nf.region", env::region());
  put_if_nonempty(&common_tags_, "nf.account", env::account());
  return common_tags_;
}

Config::Config() noexcept
    : disabled_file_watcher_(features.disabled_file),
      common_tags_(get_default_common_tags()) {}

std::unique_ptr<Config> Config::FromJson(const rapidjson::Document& json,
                                         const Config& defaults) noexcept {
  auto config = std::make_unique<Config>();

  config->features = FeaturesConfig::FromJson(json, defaults.features);
  config->http = HttpConfig::FromJson(json, defaults.http);
  config->endpoints = EndpointConfig::FromJson(json, defaults.endpoints);
  config->logs = LogConfig::FromJson(json, defaults.logs);

  config->disabled_file_watcher_ = FileWatcher(config->features.disabled_file);
  config->common_tags_ = defaults.common_tags_;

  return config;
}

std::string Config::ToString() const noexcept {
  return fmt::format(
      "Config(endpoints={}\n, subsRefresh={}ms\n"
      ", validateMetrics={}, forceStart={}, mainEnabled={}, subsEnabled={}\n"
      ", publishCfg={}\n, log={}\n, http={}\n, commonTags={})",
      EndpointConfiguration(), SubRefreshMillis(), ShouldValidateMetrics(),
      ShouldForceStart(), IsMainEnabled(), AreSubsEnabled(), PublishConfig(),
      LogConfiguration(), HttpConfiguration(), CommonTags());
}

std::ostream& operator<<(std::ostream& os, const HttpConfig& http_config) {
  os << std::boolalpha;
  os << "H(C=" << http_config.connect_timeout
     << ",R=" << http_config.read_timeout << ",B=" << http_config.batch_size
     << ",P=" << http_config.send_in_parallel << ")";
  os << std::noboolalpha;

  return os;
}

std::ostream& operator<<(std::ostream& os, const LogConfig& log_config) {
  os << std::boolalpha;
  os << "L(V=" << log_config.verbosity << ",S=" << log_config.max_size
     << ",N=" << log_config.max_files << ",DumpM=" << log_config.dump_metrics
     << ",DumpS=" << log_config.dump_subscriptions << ")";
  os << std::noboolalpha;

  return os;
}

std::ostream& operator<<(std::ostream& os, const EndpointConfig& endpoints) {
  os << "E(P=" << endpoints.publish << ",S=" << endpoints.subscriptions
     << ",E=" << endpoints.evaluate << ")";
  return os;
}

HttpConfig HttpConfig::FromJson(const rapidjson::Document& document,
                                const HttpConfig& defaults) noexcept {
  HttpConfig config;
  config.connect_timeout = document.HasMember("connectTimeout")
                               ? document["connectTimeout"].GetInt()
                               : defaults.connect_timeout;

  config.read_timeout = document.HasMember("readTimeout")
                            ? document["readTimeout"].GetInt()
                            : defaults.read_timeout;

  config.batch_size = document.HasMember("batchSize")
                          ? document["batchSize"].GetInt()
                          : defaults.batch_size;

  config.send_in_parallel = document.HasMember("sendInParallel")
                                ? document["sendInParallel"].GetBool()
                                : defaults.send_in_parallel;

  return config;
}

EndpointConfig EndpointConfig::FromJson(
    const rapidjson::Document& document,
    const EndpointConfig& defaults) noexcept {
  EndpointConfig config;
  config.evaluate = document.HasMember("evaluateUrl")
                        ? ExpandEnvVars(document["evaluateUrl"].GetString())
                        : defaults.evaluate;

  config.subscriptions =
      document.HasMember("subscriptionsUrl")
          ? ExpandEnvVars(document["subscriptionsUrl"].GetString())
          : defaults.subscriptions;

  config.publish = document.HasMember("publishUrl")
                       ? ExpandEnvVars(document["publishUrl"].GetString())
                       : defaults.publish;

  return config;
}

LogConfig LogConfig::FromJson(const rapidjson::Document& json,
                              const LogConfig& defaults) noexcept {
  LogConfig config;
  config.verbosity = json.HasMember("logVerbosity")
                         ? json["logVerbosity"].GetInt()
                         : defaults.verbosity;

  config.max_size = json.HasMember("logMaxSize") ? json["logMaxSize"].GetUint()
                                                 : defaults.max_size;

  config.max_files = json.HasMember("logMaxFiles")
                         ? json["logMaxFiles"].GetUint()
                         : defaults.max_files;

  config.dump_metrics = json.HasMember("dumpMetrics")
                            ? json["dumpMetrics"].GetBool()
                            : defaults.dump_metrics;

  config.dump_subscriptions = json.HasMember("dumpSubscriptions")
                                  ? json["dumpSubscriptions"].GetBool()
                                  : defaults.dump_subscriptions;
  return config;
}

static std::vector<std::string> vector_from(const rapidjson::Value& array) {
  std::vector<std::string> res;
  for (auto& v : array.GetArray()) {
    res.emplace_back(v.GetString());
  }
  return res;
}

FeaturesConfig FeaturesConfig::FromJson(
    const rapidjson::Document& json, const FeaturesConfig& defaults) noexcept {
  FeaturesConfig config;
  config.validate = json.HasMember("validateMetrics")
                        ? json["validateMetrics"].GetBool()
                        : defaults.validate;

  config.publish_config = json.HasMember("publishConfig")
                              ? vector_from(json["publishConfig"])
                              : defaults.publish_config;

  config.force_start = json.HasMember("forceStart")
                           ? json["forceStart"].GetBool()
                           : defaults.force_start;

  config.main = json.HasMember("publishEnabled")
                    ? json["publishEnabled"].GetBool()
                    : defaults.main;

  config.subscriptions = json.HasMember("subscriptionsEnabled")
                             ? json["subscriptionsEnabled"].GetBool()
                             : defaults.subscriptions;

  config.subscription_refresh_ms =
      json.HasMember("subscriptionsRefreshMillis")
          ? json["subscriptionsRefreshMillis"].GetInt64()
          : defaults.subscription_refresh_ms;

  const char* maybe_file = std::getenv("ATLAS_DISABLED_FILE");
  config.disabled_file =
      maybe_file != nullptr ? maybe_file : defaults.disabled_file;

  return config;
}

}  // namespace util
}  // namespace atlas
