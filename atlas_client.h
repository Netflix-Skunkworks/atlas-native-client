#pragma once

#include "util/config.h"
#include "meter/measurement.h"
#include "meter/registry.h"
#include <memory>

namespace atlas {

class Client {
  class impl;
  std::unique_ptr<impl> impl_;

 public:
  Client() noexcept;
  ~Client() noexcept;

  void Start() noexcept;

  void Stop() noexcept;

  std::shared_ptr<util::Config> GetConfig() noexcept;

  void Push(const meter::Measurements& measurements) noexcept;

  void AddCommonTag(const char* key, const char* value) noexcept;

  void SetLoggingDirs(const std::vector<std::string>& dirs) noexcept;
  void UseConsoleLogger(int level) noexcept;
  std::shared_ptr<atlas::meter::Registry> GetRegistry() const noexcept;
};

}  // namespace atlas
