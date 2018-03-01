#pragma once

#include "config.h"
#include <spdlog/spdlog.h>
#include <vector>

namespace atlas {
namespace util {

class LogManager {
 public:
  explicit LogManager(std::string name) noexcept;
  std::shared_ptr<spdlog::logger> Logger() noexcept;
  void UseConsoleLogger(int level) noexcept;
  void SetLoggingDirs(const std::vector<std::string>& dirs) noexcept;
  void InitializeLogging(const LogConfig& config) noexcept;
  std::string GetLoggingDir() const noexcept;

 private:
  std::string name_;
  std::mutex logger_mutex;
  std::shared_ptr<spdlog::logger> current_logger_;
  std::vector<std::string> logging_directories = {"/logs/atlasd", "./logs"};

  std::string current_logging_directory;
  LogConfig current_log_config;

  std::string get_usable_logging_directory() const noexcept;
  void fallback_init_for_logger() noexcept;
  void initialize_logger(const std::string& log_dir) noexcept;
  void initialize() noexcept;
};

LogManager& log_manager() noexcept;

inline std::shared_ptr<spdlog::logger> Logger() noexcept {
  return log_manager().Logger();
}

inline void UseConsoleLogger(int level) noexcept {
  log_manager().UseConsoleLogger(level);
}

inline void SetLoggingDirs(const std::vector<std::string>& dirs) noexcept {
  log_manager().SetLoggingDirs(dirs);
}

inline void InitializeLogging(const LogConfig& config) noexcept {
  log_manager().InitializeLogging(config);
}

inline std::string GetLoggingDir() noexcept {
  return log_manager().GetLoggingDir();
}

}  // namespace util
}  // namespace atlas
