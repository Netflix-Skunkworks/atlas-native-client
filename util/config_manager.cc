#include "config_manager.h"
#include "http.h"
#include "logger.h"

#include <fstream>
#include <sstream>

namespace atlas {
namespace util {
static constexpr int64_t kRefresherMillis = 10000;
static constexpr int kConnectTimeout = 6;
static constexpr int kReadTimeout = 20;
static constexpr int kBatchSize = 10000;
static constexpr bool kValidateMetrics = true;

static const char* kEvaluateUrl =
    "http://atlas-lwcapi-iep.$EC2_REGION.iep$NETFLIX_ENVIRONMENT.netflix.net/"
    "lwc/api/"
    "v1/evaluate";
static const char* kSubscriptionsUrl =
    "http://atlas-lwcapi-iep.$EC2_REGION.iep$NETFLIX_ENVIRONMENT.netflix.net/"
    "lwc/api/"
    "v1/expressions/$NETFLIX_CLUSTER";
static const char* kPublishUrl =
    "http://"
    "atlas-pub-$EC2_OWNER_ID.$EC2_REGION.iep$NETFLIX_ENVIRONMENT.netflix.net/"
    "api/v1/publish-fast";
static const char* kCheckClusterUrl =
    "http://"
    "atlas-alert-api-$EC2_OWNER_ID.$EC2_REGION.$NETFLIX_ENVIRONMENT.netflix."
    "net/"
    "alertchecker/checkCluster/$NETFLIX_CLUSTER";
static const char* kPublishExpr = ":true,:all";
static const char* kDefaultDisabledFile = "/mnt/data/atlas.disabled";

static const std::vector<std::string> kPublishConfig{kPublishExpr};
constexpr int kDefaultVerbosity = 2;

static std::vector<std::string> vector_from(const rapidjson::Value& array) {
  std::vector<std::string> res;
  for (auto& v : array.GetArray()) {
    res.emplace_back(v.GetString());
  }
  return res;
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

static std::unique_ptr<Config> DocToConfig(
    const rapidjson::Document& document,
    std::unique_ptr<Config> defaults) noexcept {
  if (!document.IsObject()) {
    Logger()->error(
        "Expecting a JSON object while parsing a configuration entity");
    return defaults;
  }

  std::string eval_url = document.HasMember("evaluateUrl")
                             ? document["evaluateUrl"].GetString()
                             : defaults->EvalEndpoint();

  std::string sub_endpoint = document.HasMember("subscriptionsUrl")
                                 ? document["subscriptionsUrl"].GetString()
                                 : defaults->SubsEndpoint();

  std::string publish_endpoint = document.HasMember("publishUrl")
                                     ? document["publishUrl"].GetString()
                                     : defaults->PublishEndpoint();

  auto validate_metrics = document.HasMember("validateMetrics")
                              ? document["validateMetrics"].GetBool()
                              : defaults->ShouldValidateMetrics();

  std::string check_cluster_endpoint =
      document.HasMember("checkClusterUrl")
          ? document["checkClusterUrl"].GetString()
          : defaults->CheckClusterEndpoint();

  std::vector<std::string> publish_config =
      document.HasMember("publishConfig")
          ? vector_from(document["publishConfig"])
          : defaults->PublishConfig();

  auto force_start = document.HasMember("forceStart")
                         ? document["forceStart"].GetBool()
                         : defaults->ShouldForceStart();

  auto main_enabled = document.HasMember("publishEnabled")
                          ? document["publishEnabled"].GetBool()
                          : defaults->IsMainEnabled();

  auto subs_enabled = document.HasMember("subscriptionsEnabled")
                          ? document["subscriptionsEnabled"].GetBool()
                          : defaults->AreSubsEnabled();

  auto dump_metrics = document.HasMember("dumpMetrics")
                          ? document["dumpMetrics"].GetBool()
                          : defaults->ShouldDumpMetrics();

  auto dump_subscriptions = document.HasMember("dumpSubscriptions")
                                ? document["dumpSubscriptions"].GetBool()
                                : defaults->ShouldDumpSubs();

  int64_t sub_refresh = document.HasMember("subscriptionsRefreshMillis")
                            ? document["subscriptionsRefreshMillis"].GetInt64()
                            : defaults->SubRefreshMillis();

  auto connect_timeout = document.HasMember("connectTimeout")
                             ? document["connectTimeout"].GetInt()
                             : defaults->ConnectTimeout();

  auto read_timeout = document.HasMember("readTimeout")
                          ? document["readTimeout"].GetInt()
                          : defaults->ReadTimeout();

  auto batch_size = document.HasMember("batchSize")
                        ? document["batchSize"].GetInt()
                        : defaults->BatchSize();

  auto log_verbosity = document.HasMember("logVerbosity")
                           ? document["logVerbosity"].GetInt()
                           : defaults->LogVerbosity();

  auto log_max_size = document.HasMember("logMaxSize")
                          ? document["logMaxSize"].GetUint()
                          : defaults->LogMaxSize();

  auto log_max_files = document.HasMember("logMaxFiles")
                           ? document["logMaxFiles"].GetUint()
                           : defaults->LogMaxFiles();

  return std::make_unique<Config>(
      defaults->DisabledFile(), eval_url, sub_endpoint, publish_endpoint,
      validate_metrics, check_cluster_endpoint, publish_config, sub_refresh,
      connect_timeout, read_timeout, batch_size, force_start, main_enabled,
      subs_enabled, dump_metrics, dump_subscriptions, log_verbosity,
      log_max_size, log_max_files, get_default_common_tags());
}

static std::unique_ptr<Config> ParseConfigFile(
    const std::string& file_name, std::unique_ptr<Config> defaults) noexcept {
  if (access(file_name.c_str(), R_OK) == 0) {
    std::ifstream cfg_file(file_name);
    std::ostringstream buffer;
    buffer << cfg_file.rdbuf();
    const auto& s = buffer.str();
    if (s.empty()) {
      Logger()->info("Parsing {} - Found an empty config", file_name);
      return defaults;
    }
    rapidjson::Document document;
    document
        .Parse<rapidjson::kParseCommentsFlag | rapidjson::kParseNanAndInfFlag>(
            buffer.str().c_str());
    auto cfg = DocToConfig(document, std::move(defaults));
    Logger()->debug("Config after parsing {}: {}", file_name,
                    ConfigToString(*cfg));
    return cfg;
  }
  return defaults;
}

std::unique_ptr<Config> DefaultConfig() noexcept {
  const char* disabled_file = std::getenv("ATLAS_DISABLED_FILE");
  if (disabled_file == nullptr) {
    disabled_file = kDefaultDisabledFile;
  }

  auto log_num_size = GetLogSizes();
  return std::make_unique<Config>(
      disabled_file, std::string(kEvaluateUrl), std::string(kSubscriptionsUrl),
      std::string(kPublishUrl), kValidateMetrics, std::string(kCheckClusterUrl),
      kPublishConfig, kRefresherMillis, kConnectTimeout, kReadTimeout,
      kBatchSize,
      // do not run on dev env
      false,
      // enable main but not subscriptions yet (need clusters in main account)
      true, false,
      // do not dump main or subs
      false, false, kDefaultVerbosity, log_num_size.max_size,
      log_num_size.max_files, get_default_common_tags());
}

static constexpr const char* const kGlobalFile =
    "/usr/local/etc/atlas-config.json";
static constexpr const char* const kLocalFile = "./atlas-config.json";

std::unique_ptr<Config> ConfigManager::get_current_config() noexcept {
  auto my_global = ParseConfigFile(kGlobalFile, DefaultConfig());
  return ParseConfigFile(kLocalFile, std::move(my_global));
}

ConfigManager::ConfigManager() noexcept
    : current_config_{std::shared_ptr<Config>(get_current_config())} {}

std::shared_ptr<Config> ConfigManager::GetConfig() const noexcept {
  std::lock_guard<std::mutex> lock{config_mutex};
  return current_config_;
}

void ConfigManager::refresher() noexcept {
  while (should_run_) {
    try {
      refresh_configs();
    } catch (std::exception& e) {
      Logger()->info("Ignoring exception while refreshing configuration: {}",
                     e.what());
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(kRefresherMillis));
  }
  Logger()->info("Stopping ConfigManager");
}

void ConfigManager::Start() noexcept {
  should_run_ = true;
  std::thread refresher_thread(&ConfigManager::refresher, this);
  refresher_thread.detach();
}

void ConfigManager::Stop() noexcept { should_run_ = false; }

static void do_logging_config(const Config& config) noexcept {
  SetLoggingLevel(config.LogVerbosity());
  auto current = GetLogSizes();
  auto new_size = config.LogMaxSize();
  auto new_files = config.LogMaxFiles();
  if (current.max_files != new_files || current.max_size != new_size) {
    SetLogSizes({new_size, new_files});
  }
}

void ConfigManager::refresh_configs() noexcept {
  auto logger = Logger();
  auto new_config = get_current_config();
  do_logging_config(*new_config);
  std::lock_guard<std::mutex> lock{config_mutex};
  new_config->AddCommonTags(extra_tags_);
  current_config_ = std::move(new_config);
}

void ConfigManager::AddCommonTag(const char* key, const char* value) noexcept {
  extra_tags_.add(key, value);
}

void ConfigManager::SetNotifyAlertServer(bool /*unused*/) noexcept {}

}  // namespace util
}  // namespace atlas
