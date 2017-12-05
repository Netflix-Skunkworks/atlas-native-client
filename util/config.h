#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "../meter/id.h"
#include "file_watcher.h"

namespace atlas {
namespace util {

// step-size for main in milliseconds
static constexpr int kMainFrequencyMillis{60000};

class Config {
 public:
  Config(const std::string& disabled_file, const std::string& evaluate_endpoint,
         const std::string& subscriptions_endpoint,
         const std::string& publish_endpoint, bool validate_metrics,
         const std::string& check_cluster_endpoint,
         std::vector<std::string> publish_config, int64_t subscription_refresh,
         int connect_timeout, int read_timeout, int batch_size,
         bool force_start, bool enable_main, bool enable_subscriptions,
         bool dump_metrics, bool dump_subscriptions, int log_verbosity,
         size_t log_max_size, size_t log_max_files,
         meter::Tags common_tags) noexcept;

  std::string EvalEndpoint() const noexcept { return evaluate_endpoint_; }
  std::string SubsEndpoint() const noexcept { return subscriptions_endpoint_; }
  std::string CheckClusterEndpoint() const noexcept {
    return check_cluster_endpoint_;
  }
  int64_t SubRefreshMillis() const noexcept { return subscription_refresh_; }
  int ConnectTimeout() const noexcept { return connect_timeout_; }
  int ReadTimeout() const noexcept { return read_timeout_; }
  int BatchSize() const noexcept { return batch_size_; }
  std::string LoggingDirectory() const noexcept;
  bool ShouldValidateMetrics() const noexcept { return validate_metrics_; }
  bool ShouldForceStart() const noexcept { return force_start_; }
  bool IsMainEnabled() const noexcept;
  bool AreSubsEnabled() const noexcept;
  bool ShouldDumpMetrics() const noexcept { return dump_metrics_; }
  bool ShouldDumpSubs() const noexcept { return dump_subscriptions_; }
  std::string PublishEndpoint() const noexcept { return publish_endpoint_; }
  const std::vector<std::string>& PublishConfig() const noexcept {
    return publish_config_;
  }
  int LogVerbosity() const noexcept { return log_verbosity_; }
  size_t LogMaxSize() const noexcept { return log_max_size_; }
  size_t LogMaxFiles() const noexcept { return log_max_files_; }
  meter::Tags CommonTags() const noexcept { return common_tags_; }
  void AddCommonTags(const meter::Tags& extra_tags) noexcept {
    common_tags_.add_all(extra_tags);
  }
  std::string DisabledFile() const noexcept {
    return disabled_file_watcher_.file_name();
  }

  // for testing
  Config WithPublishConfig(std::vector<std::string> publish_config) {
    auto cfg = Config(*this);
    cfg.publish_config_ = std::move(publish_config);
    return cfg;
  }

 private:
  FileWatcher disabled_file_watcher_;
  std::string evaluate_endpoint_;
  std::string subscriptions_endpoint_;
  std::string publish_endpoint_;
  bool validate_metrics_;
  std::string check_cluster_endpoint_;
  std::vector<std::string> publish_config_;
  int64_t subscription_refresh_;
  int connect_timeout_;
  int read_timeout_;
  int batch_size_;
  bool force_start_;
  bool enable_main_;
  bool enable_subscriptions_;
  bool dump_metrics_;
  bool dump_subscriptions_;
  int log_verbosity_;
  size_t log_max_size_;
  size_t log_max_files_;
  meter::Tags common_tags_;
};

std::string ConfigToString(const Config& config) noexcept;

}  // namespace util
}  // namespace atlas
