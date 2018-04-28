#pragma once

#include "config.h"
#include "../meter/registry.h"
#include <rapidjson/document.h>
#include <spdlog/spdlog.h>
#include <memory>
#include <string>

namespace atlas {
namespace util {

class CurlHeaders;
class http {
 public:
  http(std::shared_ptr<meter::Registry> registry, const HttpConfig& config)
      : registry_(std::move(registry)),
        connect_timeout_(config.connect_timeout),
        read_timeout_(config.read_timeout) {}

  int conditional_get(const std::string& url, std::string* etag,
                      std::string* res) const;

  int get(const std::string& url, std::string* res) const;

  int post(const std::string& url, const char* content_type,
           const char* payload, size_t size) const;

  int post(const std::string& url, const rapidjson::Document& payload) const;

  std::vector<int> post_batches(
      const std::string& url,
      const std::vector<rapidjson::Document>& batches) const;
  static void global_init() noexcept;
  static void global_shutdown() noexcept;

 private:
  std::shared_ptr<meter::Registry> registry_;
  int connect_timeout_;
  int read_timeout_;

  int do_post(const std::string& url, std::unique_ptr<CurlHeaders> headers,
              std::unique_ptr<char[]> payload, size_t size) const;
};

}  // namespace util
}  // namespace atlas
