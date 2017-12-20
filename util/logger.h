#pragma once

#include "config.h"
#include <spdlog/spdlog.h>
#include <vector>

namespace atlas {
namespace util {

std::shared_ptr<spdlog::logger> Logger() noexcept;
void UseConsoleLogger(int level) noexcept;
void SetLoggingDirs(const std::vector<std::string>& dirs) noexcept;
void InitializeLogging(const LogConfig& config) noexcept;
std::string GetLoggingDir() noexcept;

}  // namespace util
}  // namespace atlas
