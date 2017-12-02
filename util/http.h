#pragma once

#include <rapidjson/document.h>
#include <spdlog/spdlog.h>
#include <memory>
#include <string>

namespace atlas {

namespace util {

class http {
 public:
  int conditional_get(const std::string& url, std::string& etag,
                      int connect_timeout, int read_timeout,
                      std::string& res) const;

  int get(const std::string& url, int connect_timeout, int read_timeout,
          std::string& res) const;

  int post(const std::string& url, int connect_timeout, int read_timeout,
           const char* content_type, const char* payload, size_t size) const;

  int post(const std::string& url, int connect_timeout, int read_timeout,
           const rapidjson::Document& payload) const;

  static void global_init() noexcept;
  static void global_shutdown() noexcept;
};
}
}
