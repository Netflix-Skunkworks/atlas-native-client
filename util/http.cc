#include "http.h"
#include "gzip.h"
#include "json.h"
#include "logger.h"
#include "memory.h"
#include "strings.h"

#include <curl/curl.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace atlas {
namespace util {

namespace {

class CurlHeaders {
 public:
  CurlHeaders() = default;
  ~CurlHeaders() { curl_slist_free_all(list_); }
  CurlHeaders(const CurlHeaders&) = delete;
  CurlHeaders(CurlHeaders&&) = delete;
  CurlHeaders& operator=(const CurlHeaders&) = delete;
  CurlHeaders& operator=(CurlHeaders&&) = delete;
  void append(const std::string& string) {
    list_ = curl_slist_append(list_, string.c_str());
  }
  curl_slist* headers() { return list_; }

 private:
  curl_slist* list_{nullptr};
};

constexpr const char* const kUserAgent = "atlas-native/1.0";

class CurlHandle {
 public:
  CurlHandle() noexcept : handle_{curl_easy_init()} {
    curl_easy_setopt(handle_, CURLOPT_ACCEPT_ENCODING, "gzip");
    curl_easy_setopt(handle_, CURLOPT_USERAGENT, kUserAgent);
  }

  CurlHandle(const CurlHandle&) = delete;

  CurlHandle& operator=(const CurlHandle&) = delete;

  CurlHandle(CurlHandle&& other) = delete;

  CurlHandle& operator=(CurlHandle&& other) = delete;

  ~CurlHandle() {
    // nullptr is handled by curl
    curl_easy_cleanup(handle_);
  }

  CURL* handle() const noexcept { return handle_; }

  CURLcode perform() { return curl_easy_perform(handle()); }

  CURLcode set_opt(CURLoption option, const void* param) {
    return curl_easy_setopt(handle(), option, param);
  }

  int status_code() {
    // curl requires this to be a long
    long http_code = 400;
    curl_easy_getinfo(handle(), CURLINFO_RESPONSE_CODE, &http_code);
    return static_cast<int>(http_code);
  }

  void set_url(const std::string& url) { set_opt(CURLOPT_URL, url.c_str()); }

  void set_headers(std::unique_ptr<CurlHeaders> headers) {
    headers_ = std::move(headers);
    set_opt(CURLOPT_HTTPHEADER, headers_->headers());
  }

  void set_connect_timeout(int connect_timeout_seconds) {
    curl_easy_setopt(handle_, CURLOPT_CONNECTTIMEOUT,
                     (long)connect_timeout_seconds);
  }

  void set_read_timeout(int read_timeout_seconds) {
    curl_easy_setopt(handle_, CURLOPT_TIMEOUT, (long)read_timeout_seconds);
  }

  void post_payload(const Bytef* payload, size_t size) {
    curl_easy_setopt(handle_, CURLOPT_POST, 1L);
    curl_easy_setopt(handle_, CURLOPT_POSTFIELDS, payload);
    curl_easy_setopt(handle_, CURLOPT_POSTFIELDSIZE, size);
  }

 private:
  CURL* handle_;
  std::unique_ptr<CurlHeaders> headers_;
};

class Buffer {
 public:
  Buffer() : memory(static_cast<char*>(malloc(1))) {}
  ~Buffer() { free(memory); }
  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;
  Buffer& operator=(Buffer&&) = delete;
  Buffer(Buffer&&) = delete;
  void append(void* data, size_t data_size) {
    memory = static_cast<char*>(realloc(memory, size + data_size + 1));
    if (memory == nullptr) {
      throw std::bad_alloc();
    }

    memcpy(memory + size, data, data_size);
    size += data_size;
    memory[size] = 0;
  }
  void assign(std::string* s) { s->assign(memory, size); }

 private:
  char* memory;
  size_t size = 0;
};

size_t write_memory_callback(void* contents, size_t size, size_t nmemb,
                             void* userp) {
  auto real_size = size * nmemb;
  auto buffer = static_cast<Buffer*>(userp);
  buffer->append(contents, real_size);
  return real_size;
}

size_t header_callback(char* buffer, size_t size, size_t n_items,
                       void* userdata) {
  std::string header{buffer, size * n_items};
  if (util::IStartsWith(header, "Etag: ")) {
    header.erase(0, 6);  // remove Etag:
    util::TrimRight(&header);
    Logger()->debug("Setting Etag to {}", header);
    auto etag = static_cast<std::string*>(userdata);
    etag->assign(header);
  }
  return size * n_items;
}

}  // namespace

int http::conditional_get(const std::string& url, std::string& etag,
                          std::string* res) const {
  auto logger = Logger();
  logger->debug("Conditionally getting url: {} etag: {}", url, etag);
  CurlHandle curl;
  // url to get
  curl.set_url(url);
  // send all data to this function
  curl.set_opt(CURLOPT_WRITEFUNCTION,
               reinterpret_cast<const void*>(write_memory_callback));
  if (!etag.empty()) {
    std::string ifNone = std::string("If-None-Match: ") + etag;
    auto headers = std::make_unique<CurlHeaders>();
    headers->append(ifNone);
    curl.set_headers(std::move(headers));
  }

  Buffer buffer;
  curl.set_connect_timeout(connect_timeout_);
  curl.set_read_timeout(read_timeout_);
  // pass chunk to the callback function
  curl.set_opt(CURLOPT_WRITEDATA, &buffer);
  curl.set_opt(CURLOPT_HEADERDATA, &etag);
  curl.set_opt(CURLOPT_HEADERFUNCTION,
               reinterpret_cast<void*>(header_callback));
  auto curl_res = curl.perform();
  int http_code = 400;
  bool error = false;

  if (curl_res != CURLE_OK) {
    logger->error("Failed to get {} {}", url, curl_easy_strerror(curl_res));
    error = true;
  } else {
    http_code = curl.status_code();
    if (http_code != 304) {  // if we got something back
      buffer.assign(res);
    }
  }
  if (!error) {
    logger->debug("Was able to fetch {} - status code: ", url, http_code);
    if (http_code == 0) {
      http_code = 200;
    }  // for file:///
  }
  return http_code;
}

int http::get(const std::string& url, std::string* res) const {
  auto logger = Logger();
  logger->debug("Getting url: {}", url);
  CurlHandle curl;
  // url to get
  curl.set_url(url);
  // send all data to this function
  curl.set_opt(CURLOPT_WRITEFUNCTION,
               reinterpret_cast<const void*>(write_memory_callback));

  Buffer buffer;
  // pass chunk to the callback function
  curl.set_opt(CURLOPT_WRITEDATA, &buffer);
  curl.set_connect_timeout(connect_timeout_);
  curl.set_read_timeout(read_timeout_);
  auto curl_res = curl.perform();
  auto http_code = 400;
  auto error = false;

  if (curl_res != CURLE_OK) {
    logger->error("Failed to get {} {}", url, curl_easy_strerror(curl_res));
    error = true;
  } else {
    http_code = curl.status_code();
    buffer.assign(res);
  }
  if (!error) {
    logger->debug("Was able to fetch {} - status code: {}", url, http_code);
    // for file:///
    if (http_code == 0) {
      http_code = 200;
    }
  }
  return http_code;
}

static int do_post(const std::string& url, int connect_timeout,
                   int read_timeout, std::unique_ptr<CurlHeaders> headers,
                   const Bytef* payload, size_t size) {
  CurlHandle curl;
  curl.set_connect_timeout(connect_timeout);
  curl.set_read_timeout(read_timeout);

  auto logger = Logger();
  logger->info("POSTing to url: {}", url);
  curl.set_url(url);
  curl.set_headers(std::move(headers));
  curl.post_payload(payload, size);

  auto curl_res = curl.perform();
  auto error = false;
  auto http_code = 400;

  if (curl_res != CURLE_OK) {
    logger->error("Failed to POST {}: {}", url, curl_easy_strerror(curl_res));
    error = true;
  } else {
    http_code = curl.status_code();
  }

  if (!error) {
    logger->info("Was able to POST to {} - status code: {}", url, http_code);
  }
  return http_code;
}

int http::post(const std::string& url, const char* content_type,
               const char* payload, size_t size) const {
  auto headers = std::make_unique<CurlHeaders>();
  headers->append(content_type);

  if (size <= 16) {
    return do_post(url, connect_timeout_, read_timeout_, std::move(headers),
                   reinterpret_cast<const Bytef*>(payload), size);
  }

  headers->append("Content-Encoding: gzip");
  auto compressed_size = compressBound(size) + 16;
  auto compressed_payload =
      std::unique_ptr<Bytef[]>(new Bytef[compressed_size]);
  auto compress_res =
      gzip_compress(compressed_payload.get(), &compressed_size,
                    reinterpret_cast<const Bytef*>(payload), size);
  if (compress_res != Z_OK) {
    Logger()->error(
        "Failed to compress payload: {}, while posting to {} - uncompressed "
        "size: {}",
        compress_res, url, size);
    return 400;
  }

  return do_post(url, connect_timeout_, read_timeout_, std::move(headers),
                 compressed_payload.get(), compressed_size);
}

static constexpr const char* const json_type = "Content-Type: application/json";
int http::post(const std::string& url,
               const rapidjson::Document& payload) const {
  rapidjson::StringBuffer buffer;
  auto c_str = JsonGetString(buffer, payload);
  return post(url, json_type, c_str, strlen(c_str));
}

void http::global_init() noexcept { curl_global_init(CURL_GLOBAL_ALL); }

void http::global_shutdown() noexcept { curl_global_cleanup(); }

}  // namespace util
}  // namespace atlas
