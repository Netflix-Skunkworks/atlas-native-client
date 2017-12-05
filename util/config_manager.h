#pragma once

#include <atomic>
#include <rapidjson/document.h>
#include <memory>
#include <mutex>
#include <string>
#include "config.h"
#include "environment.h"

namespace atlas {
namespace util {

std::unique_ptr<Config> DefaultConfig() noexcept;

class ConfigManager {
 public:
  ConfigManager() noexcept;
  ~ConfigManager() { Stop(); }
  std::shared_ptr<Config> GetConfig() const noexcept;

  void Start() noexcept;

  void Stop() noexcept;

  void AddCommonTag(const char* key, const char* value) noexcept;

  // deprecated - alert server uses the streaming path to check on-instance
  // alerts so this is no longer needed
  void SetNotifyAlertServer(bool /*unused*/) noexcept;

 private:
  mutable std::mutex config_mutex;
  std::shared_ptr<Config> current_config_;
  std::atomic<bool> should_run_{false};
  meter::Tags extra_tags_;

  void refresher() noexcept;
  void refresh_configs() noexcept;
  std::unique_ptr<Config> get_current_config() noexcept;
};
}  // namespace util
}  // namespace atlas
