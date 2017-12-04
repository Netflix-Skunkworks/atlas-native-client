#pragma once

#include "config.h"
#include <rapidjson/document.h>
#include <spdlog/spdlog.h>
#include <memory>
#include <string>

namespace atlas {
namespace util {

class http {
 public:
  explicit http(const Config& config)
      : connect_timeout_(config.ConnectTimeout()),
        read_timeout_(config.ReadTimeout()) {}

  int conditional_get(const std::string& url, std::string& etag,
                      std::string* res) const;

  int get(const std::string& url, std::string* res) const;

  int post(const std::string& url, const char* content_type,
           const char* payload, size_t size) const;

  int post(const std::string& url, const rapidjson::Document& payload) const;

  static void global_init() noexcept;
  static void global_shutdown() noexcept;

 private:
  int connect_timeout_;
  int read_timeout_;
};

}  // namespace util
}  // namespace atlas
