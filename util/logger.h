#pragma once

#include <spdlog/spdlog.h>
#include <vector>

namespace atlas {
namespace util {

std::shared_ptr<spdlog::logger> Logger() noexcept;
void UseConsoleLogger() noexcept;
void SetLoggingLevel(int level) noexcept;
void SetLoggingDirs(const std::vector<std::string>& dirs) noexcept;
std::string GetLoggingDir() noexcept;

}  // namespace util
}  // namespace atlas
