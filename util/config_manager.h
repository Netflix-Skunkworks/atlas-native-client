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

std::unique_ptr<Config> DefaultConfig(bool default_notify = true) noexcept;

class ConfigManager {
 public:
  ConfigManager() noexcept;
  ~ConfigManager() { Stop(); }
  std::shared_ptr<Config> GetConfig() const noexcept;

  void Start() noexcept;

  void Stop() noexcept;

  void AddCommonTag(const char* key, const char* value) noexcept;

  void SetNotifyAlertServer(bool notify) noexcept;

 private:
  mutable std::mutex config_mutex;
  std::shared_ptr<Config> current_config_;
  std::atomic<bool> should_run_{false};
  meter::Tags extra_tags_;
  bool default_notify_ = true;

  void refresher() noexcept;
  void refresh_configs() noexcept;
  std::unique_ptr<Config> get_current_config() noexcept;
};
}
}
