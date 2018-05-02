#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "../meter/id.h"
#include "file_watcher.h"
#include "strings.h"

namespace rapidjson {

template <typename CharType>
struct UTF8;

class CrtAllocator;

template <typename BaseAllocator>
class MemoryPoolAllocator;

template <typename Encoding, typename Allocator, typename StackAllocator>
class GenericDocument;

typedef GenericDocument<UTF8<char>, MemoryPoolAllocator<CrtAllocator>,
                        CrtAllocator>
    Document;
}  // namespace rapidjson

namespace atlas {
namespace util {

// step-size for main in milliseconds
static constexpr int kMainFrequencyMillis{60000};

struct HttpConfig {
  // timeout in seconds
  int connect_timeout = 6;
  int read_timeout = 20;
  int batch_size = 10000;
  bool send_in_parallel = false;

  static HttpConfig FromJson(const rapidjson::Document& json,
                             const HttpConfig& defaults) noexcept;
};
std::ostream& operator<<(std::ostream& os, const HttpConfig& http_config);

struct EndpointConfig {
  std::string subscriptions = ExpandEnvVars(
      "http://atlas-lwcapi-iep.$EC2_REGION.iep$NETFLIX_ENVIRONMENT.netflix.net/"
      "lwc/api/"
      "v1/expressions/$NETFLIX_CLUSTER");
  std::string publish = ExpandEnvVars(
      "http://"
      "atlas-pub-$EC2_OWNER_ID.$EC2_REGION.iep$NETFLIX_ENVIRONMENT.netflix.net/"
      "api/v1/publish-fast");
  std::string evaluate = ExpandEnvVars(
      "http://atlas-lwcapi-iep.$EC2_REGION.iep$NETFLIX_ENVIRONMENT.netflix.net/"
      "lwc/api/"
      "v1/evaluate");

  static EndpointConfig FromJson(const rapidjson::Document& json,
                                 const EndpointConfig& defaults) noexcept;
};
std::ostream& operator<<(std::ostream& os, const EndpointConfig& endpoints);

struct LogConfig {
  int verbosity = 2;
  size_t max_size = 1024u * 1024u;
  size_t max_files = 8;
  bool dump_metrics = false;
  bool dump_subscriptions = false;

  static LogConfig FromJson(const rapidjson::Document& json,
                            const LogConfig& defaults) noexcept;
};

std::ostream& operator<<(std::ostream& os, const LogConfig& log_config);

struct FeaturesConfig {
  bool force_start = false;
  bool validate = true;
  bool main = true;
  bool subscriptions = false;
  int64_t subscription_refresh_ms = 10000;
  std::vector<std::string> publish_config = {":true,:all"};
  std::string disabled_file = "/mnt/data/atlas.disabled";

  static FeaturesConfig FromJson(const rapidjson::Document& json,
                                 const FeaturesConfig& defaults) noexcept;
};

class Config {
 public:
  Config() noexcept;
  static std::unique_ptr<Config> FromJson(const rapidjson::Document& json,
                                          const Config& defaults) noexcept;
  std::string ToString() const noexcept;

  const LogConfig& LogConfiguration() const noexcept { return logs; }
  const HttpConfig& HttpConfiguration() const noexcept { return http; }
  const EndpointConfig& EndpointConfiguration() const noexcept {
    return endpoints;
  }

  const std::vector<std::string>& PublishConfig() const noexcept {
    return features.publish_config;
  }
  int64_t SubRefreshMillis() const noexcept {
    return features.subscription_refresh_ms;
  }
  std::string LoggingDirectory() const noexcept;
  bool ShouldValidateMetrics() const noexcept { return features.validate; }
  bool ShouldForceStart() const noexcept { return features.force_start; }
  bool IsMainEnabled() const noexcept;
  bool AreSubsEnabled() const noexcept;
  meter::Tags CommonTags() const noexcept { return common_tags_; }
  void AddCommonTags(const meter::Tags& extra_tags) noexcept {
    common_tags_.add_all(extra_tags);
  }

  // for testing
  Config WithPublishConfig(std::vector<std::string> publish_config) {
    auto cfg = Config(*this);
    cfg.features.publish_config = std::move(publish_config);
    return cfg;
  }

 private:
  FeaturesConfig features;
  EndpointConfig endpoints;
  LogConfig logs;
  HttpConfig http;

  FileWatcher disabled_file_watcher_;
  meter::Tags common_tags_;
};

}  // namespace util
}  // namespace atlas
