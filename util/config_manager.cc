#include "config_manager.h"
#include "http.h"
#include "logger.h"

#include <fstream>
#include <sstream>

namespace atlas {
namespace util {

static std::unique_ptr<Config> DocToConfig(
    const rapidjson::Document& document,
    std::unique_ptr<Config> defaults) noexcept {
  if (!document.IsObject()) {
    Logger()->error(
        "Expecting a JSON object while parsing a configuration entity");
    return defaults;
  }

  return Config::FromJson(document, *defaults);
}

static std::unique_ptr<Config> ParseConfigFile(
    const std::string& file_name, std::unique_ptr<Config> defaults) noexcept {
  if (access(file_name.c_str(), R_OK) == 0) {
    const auto& logger = Logger();
    std::ifstream cfg_file(file_name);
    std::ostringstream buffer;
    buffer << cfg_file.rdbuf();
    const auto& s = buffer.str();
    if (s.empty()) {
      logger->info("Parsing {} - Found an empty config", file_name);
      return defaults;
    }
    rapidjson::Document document;
    document
        .Parse<rapidjson::kParseCommentsFlag | rapidjson::kParseNanAndInfFlag>(
            buffer.str().c_str());
    auto cfg = DocToConfig(document, std::move(defaults));
    logger->debug("Config after parsing {}: {}", file_name, cfg->ToString());
    return cfg;
  }
  return defaults;
}

static constexpr const char* const kGlobalFile =
    "/usr/local/etc/atlas-config.json";

std::unique_ptr<Config> ConfigManager::get_current_config() noexcept {
  auto defaults = std::make_unique<Config>();
  defaults->SetNotify(default_notify_);
  auto my_global = ParseConfigFile(kGlobalFile, std::move(defaults));
  return ParseConfigFile(local_file_name_, std::move(my_global));
}

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
    std::unique_lock<std::mutex> lock{cv_mutex};
    cv.wait_for(lock, std::chrono::milliseconds(refresh_ms_));
  }
  Logger()->info("Stopping ConfigManager");
}

void ConfigManager::Start() noexcept {
  should_run_ = true;
  refresh_configs();
  refresher_thread = std::thread(&ConfigManager::refresher, this);
}

void ConfigManager::Stop() noexcept {
  if (should_run_) {
    should_run_ = false;
    cv.notify_all();
    refresher_thread.join();
  }
}

static void do_logging_config(const LogConfig& config) noexcept {
  InitializeLogging(config);
}

void ConfigManager::refresh_configs() noexcept {
  auto new_config = get_current_config();
  do_logging_config(new_config->LogConfiguration());
  std::lock_guard<std::mutex> lock{config_mutex};
  new_config->AddCommonTags(extra_tags_);
  current_config_ = std::move(new_config);
}

void ConfigManager::AddCommonTag(const char* key, const char* value) noexcept {
  extra_tags_.add(key, value);
}

void ConfigManager::SetNotifyAlertServer(bool notify) noexcept {
  default_notify_ = notify;
}

}  // namespace util
}  // namespace atlas
