#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace atlas {
namespace util {

// step-size for main in milliseconds
static constexpr int kMainFrequencyMillis{60000};

class Config {
 public:
  Config(const std::string& evaluate_endpoint,
         const std::string& subscriptions_endpoint,
         const std::string& publish_endpoint, bool validate_metrics,
         const std::string& check_cluster_endpoint, bool notifyAlertServer,
         std::vector<std::string> publish_config, int64_t subscription_refresh,
         int connect_timeout, int read_timeout, int batch_size,
         bool force_start, bool enable_main, bool enable_subscriptions,
         bool dump_metrics, bool dump_subscriptions, int log_verbosity,
         std::map<std::string, std::string> common_tags) noexcept;

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
  bool ShouldNotifyAlertServer() const noexcept { return notify_alert_server_; }
  bool IsMainEnabled() const noexcept { return enable_main_; }
  bool AreSubsEnabled() const noexcept { return enable_subscriptions_; }
  bool ShouldDumpMetrics() const noexcept { return dump_metrics_; }
  bool ShouldDumpSubs() const noexcept { return dump_subscriptions_; }
  std::string PublishEndpoint() const noexcept { return publish_endpoint_; }
  const std::vector<std::string>& PublishConfig() const noexcept {
    return publish_config_;
  }
  int LogVerbosity() const noexcept { return log_verbosity_; }
  std::map<std::string, std::string> CommonTags() const noexcept {
    return common_tags_;
  }
  void AddCommonTags(
      const std::map<std::string, std::string>& extra_tags) noexcept {
    common_tags_.insert(extra_tags.begin(), extra_tags.end());
  }

 private:
  std::string evaluate_endpoint_;
  std::string subscriptions_endpoint_;
  std::string publish_endpoint_;
  bool validate_metrics_;
  std::string check_cluster_endpoint_;
  bool notify_alert_server_;
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
  std::map<std::string, std::string> common_tags_;
};

std::string ConfigToString(const Config& config) noexcept;

}  // namespace util
}  // namespace atlas
