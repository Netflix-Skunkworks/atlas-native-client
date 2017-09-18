#include "config.h"
#include "dump.h"
#include "logger.h"
#include "strings.h"
#include <iomanip>
#include <sstream>

namespace atlas {
namespace util {

Config::Config(const std::string& evaluate_endpoint,
               const std::string& subscriptions_endpoint,
               const std::string& publish_endpoint, bool validate_metrics,
               const std::string& check_cluster_endpoint,
               bool notifyAlertServer, std::vector<std::string> publish_config,
               int64_t subscription_refresh, int connect_timeout,
               int read_timeout, int batch_size, bool force_start,
               bool enable_main, bool enable_subscriptions, bool dump_metrics,
               bool dump_subscriptions, int log_verbosity,
               meter::Tags common_tags) noexcept
    : evaluate_endpoint_(ExpandEnvVars(evaluate_endpoint)),
      subscriptions_endpoint_(ExpandEnvVars(subscriptions_endpoint)),
      publish_endpoint_(ExpandEnvVars(publish_endpoint)),
      validate_metrics_(validate_metrics),
      check_cluster_endpoint_(ExpandEnvVars(check_cluster_endpoint)),
      notify_alert_server_(notifyAlertServer),
      publish_config_(std::move(publish_config)),
      subscription_refresh_(subscription_refresh),
      connect_timeout_(connect_timeout),
      read_timeout_(read_timeout),
      batch_size_(batch_size),
      force_start_(force_start),
      enable_main_(enable_main),
      enable_subscriptions_(enable_subscriptions),
      dump_metrics_(dump_metrics),
      dump_subscriptions_(dump_subscriptions),
      log_verbosity_(log_verbosity),
      common_tags_(std::move(common_tags)) {}

std::string Config::LoggingDirectory() const noexcept {
  return GetLoggingDir();
}

std::string ConfigToString(const Config& config) noexcept {
  std::ostringstream os;
  os << std::boolalpha;
  os << "Config{eval_endpoint=" << config.EvalEndpoint() << '\n'
     << ", subs_endpoint=" << config.SubsEndpoint() << '\n'
     << ", subs_refresh=" << config.SubRefreshMillis() << "ms"
     << ", publish=" << config.PublishEndpoint() << '\n'
     << ", validateMetrics=" << config.ShouldValidateMetrics() << '\n'
     << ", forceStart=" << config.ShouldForceStart()
     << ", mainEnabled=" << config.IsMainEnabled() << '\n'
     << ", subsEnabled=" << config.AreSubsEnabled() << '\n'
     << ", publish_cfg=";
  const auto& pub_cfg = config.PublishConfig();
  dump_vector(os, pub_cfg);
  os << ", dump_metrics=" << config.ShouldDumpMetrics()
     << ", dump_subs=" << config.ShouldDumpSubs()
     << ", batch=" << config.BatchSize()
     << ", Timeouts(C=" << config.ConnectTimeout()
     << ",R=" << config.ReadTimeout()
     << "), logVerbosity=" << config.LogVerbosity() << ")\n"
     << ", common-tags=";
  dump_tags(os, config.CommonTags());
  os << "}";
  os << std::noboolalpha;
  return os.str();
}

}  // namespace util
}  // namespace atlas
