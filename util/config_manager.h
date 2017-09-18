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

 private:
  mutable std::mutex config_mutex;
  std::shared_ptr<Config> current_config_;
  std::atomic<bool> should_run_{false};
  meter::Tags extra_tags_;

  void refresher() noexcept;
  void refresh_configs() noexcept;
};
}
}
