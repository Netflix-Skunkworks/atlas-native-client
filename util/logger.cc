#include "logger.h"
#include <iostream>

namespace atlas {
namespace util {

static constexpr const char* const kMainLogger = "atlas_main";

LogManager& log_manager() noexcept {
  static auto* the_log_manager = new LogManager(kMainLogger);
  return *the_log_manager;
}

LogManager::LogManager(std::string name) noexcept : name_{std::move(name)} {}

static bool is_writable_dir(const std::string& dir) {
  struct stat dir_stat;
  const char* cdir = dir.c_str();
  if (stat(cdir, &dir_stat) != 0 || !S_ISDIR(dir_stat.st_mode)) {
    mkdir(cdir, 0777);
  }

  bool error = stat(cdir, &dir_stat) != 0;  // couldn't even stat it
  error |= !S_ISDIR(dir_stat.st_mode);      // or not a dir
  error |= S_ISDIR(dir_stat.st_mode) &&
           access(cdir, W_OK) != 0;  // dir, but can't write to it
  return !error;
}

static bool writable_or_missing(const std::string& file) {
  bool exists = access(file.c_str(), F_OK) == 0;
  if (!exists) {
    // missing
    return true;
  }

  // exists, check whether we can write to it
  return access(file.c_str(), W_OK) == 0;
}

static constexpr const char* const kLogFileName = "atlasclient.log";
std::string LogManager::get_usable_logging_directory() const noexcept {
  for (const auto& log_dir : logging_directories) {
    if (is_writable_dir(log_dir) &&
        writable_or_missing(join_path(log_dir, kLogFileName))) {
      return log_dir;
    }
  }
  return "/tmp";
}

void LogManager::fallback_init_for_logger() noexcept {
  if (current_logger_) {
    spdlog::drop(name_);
  }

  current_logger_ = spdlog::stderr_color_mt(name_);
  current_logging_directory = "";
}

void LogManager::initialize_logger(const std::string& log_dir) noexcept {
  const auto& file_name = join_path(log_dir, kLogFileName);
  if (writable_or_missing(file_name)) {
    spdlog::set_error_handler([](const std::string& msg) {
      std::cerr << "Log error: " << msg << "\n";
    });
    std::cerr << "Log DIR: " << log_dir << "\n";
    current_logger_ = spdlog::rotating_logger_mt(
        kMainLogger, join_path(log_dir, "atlasclient.log"),
        current_log_config.max_size, current_log_config.max_files);
    current_logging_directory = log_dir;
    current_logger_->flush_on(spdlog::level::info);
  } else {
    fallback_init_for_logger();
  }
}

void LogManager::initialize() noexcept {
  try {
    initialize_logger(get_usable_logging_directory());
  } catch (const spdlog::spdlog_ex& ex) {
    std::cerr << "Log initialization failed: " << ex.what() << "\n";
  }
}

std::shared_ptr<spdlog::logger> LogManager::Logger() noexcept {
  std::lock_guard<std::mutex> lock(logger_mutex);
  if (current_logger_) {
    return current_logger_;
  }

  current_logger_ = spdlog::get(name_);
  if (current_logger_) {
    return current_logger_;
  }

  initialize();
  return current_logger_;
}

static spdlog::level::level_enum level_from_int(int l) {
  if (l < 0 || l > static_cast<int>(spdlog::level::off)) {
    l = 2;  // default
  }
  return static_cast<spdlog::level::level_enum>(l);
}

void LogManager::SetLoggingDirs(const std::vector<std::string>& dirs) noexcept {
  std::lock_guard<std::mutex> lock(logger_mutex);
  if (current_logger_) {
    spdlog::drop(name_);
    current_logging_directory = "";
  }

  if (!dirs.empty()) {
    logging_directories.assign(dirs.begin(), dirs.end());
  }
  initialize();
}

void LogManager::UseConsoleLogger(int level) noexcept {
  std::lock_guard<std::mutex> lock(logger_mutex);
  if (current_logger_) {
    spdlog::drop(name_);
  }

  current_logger_ = spdlog::stdout_color_mt(kMainLogger);
  current_logger_->set_level(level_from_int(level));
  current_logging_directory = "";
}

void LogManager::InitializeLogging(const LogConfig& config) noexcept {
  // we can update the log level without dropping the logger
  if (current_logger_) {
    current_logger_->set_level(level_from_int(config.verbosity));
  }

  auto new_size = config.max_size;
  auto new_files = config.max_files;
  if (current_log_config.max_files == new_files ||
      current_log_config.max_size == new_size) {
    return;
  }

  // if we need to update the number or size of log files
  // then we need to drop the logger and recreate it
  current_log_config = config;
  std::lock_guard<std::mutex> lock(logger_mutex);
  if (current_logger_) {
    spdlog::drop(name_);
  }

  initialize();
}

std::string LogManager::GetLoggingDir() const noexcept {
  return current_logging_directory;
}

}  // namespace util
}  // namespace atlas
