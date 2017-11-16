#pragma once

#include <spdlog/spdlog.h>
#include <vector>

namespace atlas {
namespace util {

std::shared_ptr<spdlog::logger> Logger() noexcept;
void UseConsoleLogger(int level) noexcept;
void SetLoggingLevel(int level) noexcept;
void SetLoggingDirs(const std::vector<std::string>& dirs) noexcept;

struct LogNumSize {
  size_t max_size;
  size_t max_files;
};

void SetLogSizes(const LogNumSize& log_num_size) noexcept;
LogNumSize GetLogSizes() noexcept;
std::string GetLoggingDir() noexcept;

}  // namespace util
}  // namespace atlas
