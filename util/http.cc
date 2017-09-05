#include "http.h"
#include "../types.h"
#include "gzip.h"
#include "json.h"
#include "logger.h"
#include "strings.h"

#include <curl/curl.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace atlas {
namespace util {

namespace {

struct MemStruct {
  void* memory;
  size_t size;
};

size_t write_memory_callback(void* contents, size_t size, size_t nmemb,
                             void* userp) {
  auto real_size = size * nmemb;
  auto mem = static_cast<MemStruct*>(userp);
  mem->memory = realloc(mem->memory, mem->size + real_size + 1);
  if (mem->memory == nullptr) {
    throw std::bad_alloc();
  }

  auto memory = static_cast<char*>(mem->memory);
  memcpy(memory + mem->size, contents, real_size);
  mem->size += real_size;
  memory[mem->size] = 0;
  return real_size;
}
}  // namespace

constexpr const char* const kUserAgent = "atlas-native/1.0";

static void SetOptions(void* curl, int connect_timeout, int read_timeout) {
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, (long)connect_timeout);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)read_timeout);
  curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip");
  curl_easy_setopt(curl, CURLOPT_USERAGENT, kUserAgent);
}

static size_t header_callback(char* buffer, size_t size, size_t n_items,
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

int http::conditional_get(const std::string& url, std::string& etag,
                          int connect_timeout, int read_timeout,
                          std::string& res) const {
  auto logger = Logger();
  logger->debug("Conditionally getting url: {} etag: {}", url, etag);
  auto curl = curl_easy_init();
  // url to get
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  // send all data to this function
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
  if (!etag.empty()) {
    std::string ifNone = std::string("If-None-Match: ") + etag;
    auto headers = curl_slist_append(nullptr, ifNone.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  }

  MemStruct chunk{malloc(1), 0};  // we'll realloc as needed
  SetOptions(curl, connect_timeout, read_timeout);
  // pass chunk to the callback function
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &etag);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
  auto curl_res = curl_easy_perform(curl);
  // due to curl_easy_getinfo taking a long*
  long http_code = 400;
  bool error = false;

  if (curl_res != CURLE_OK) {
    logger->error("Failed to get {} {}", url, curl_easy_strerror(curl_res));
    error = true;
  } else {
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code != 304) {  // if we got something back
      res.assign(static_cast<char*>(chunk.memory), chunk.size);
    }
  }
  free(chunk.memory);
  curl_easy_cleanup(curl);
  if (!error) {
    logger->debug("Was able to fetch {} - status code: ", url, http_code);
    if (http_code == 0) {
      http_code = 200;
    }  // for file:///
  }
  return static_cast<int>(http_code);
}

int http::get(const std::string& url, int connect_timeout, int read_timeout,
              std::string& res) const {
  auto logger = Logger();
  logger->debug("Getting url: {}", url);
  auto curl = curl_easy_init();
  // url to get
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  // send all data to this function
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);

  MemStruct chunk{malloc(1),  // we'll realloc as needed
                  0};
  // pass chunk to the callback function
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
  SetOptions(curl, connect_timeout, read_timeout);
  auto curl_res = curl_easy_perform(curl);
  long http_code = 400;
  bool error = false;

  if (curl_res != CURLE_OK) {
    logger->error("Failed to get {} {}", url, curl_easy_strerror(curl_res));
    error = true;
  } else {
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    res.assign(static_cast<char*>(chunk.memory), chunk.size);
  }
  free(chunk.memory);
  curl_easy_cleanup(curl);
  if (!error) {
    logger->debug("Was able to fetch {} - status code: {}", url, http_code);
    // for file:///
    if (http_code == 0) {
      http_code = 200;
    }
  }
  return static_cast<int>(http_code);
}

static int do_post(const char* url, int connect_timeout, int read_timeout,
                   curl_slist* headers, const Bytef* payload, size_t size) {
  auto curl = curl_easy_init();

  auto logger = Logger();
  logger->info("POSTing to url: {}", url);
  SetOptions(curl, connect_timeout, read_timeout);
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, size);

  auto curl_res = curl_easy_perform(curl);
  bool error = false;
  long http_code = 400;

  if (curl_res != CURLE_OK) {
    logger->error("Failed to POST {}: {}", url, curl_easy_strerror(curl_res));
    error = true;
  } else {
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
  }

  curl_easy_cleanup(curl);
  if (!error) {
    logger->info("Was able to POST to {} - status code: {}", url, http_code);
  }
  return static_cast<int>(http_code);
}

int http::post(const std::string& url, int connect_timeout, int read_timeout,
               const char* content_type, const char* payload,
               size_t size) const {
  auto headers = curl_slist_append(nullptr, content_type);

  if (size <= 16) {
    return do_post(url.c_str(), connect_timeout, read_timeout, headers,
                   reinterpret_cast<const Bytef*>(payload), size);
  }

  headers = curl_slist_append(headers, "Content-Encoding: gzip");
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

  return do_post(url.c_str(), connect_timeout, read_timeout, headers,
                 compressed_payload.get(), compressed_size);
}

static constexpr const char* const json_type = "Content-Type: application/json";
int http::post(const std::string& url, int connect_timeout, int read_timeout,
               const rapidjson::Document& payload) const {
  rapidjson::StringBuffer buffer;
  auto c_str = JsonGetString(buffer, payload);
  return post(url, connect_timeout, read_timeout, json_type, c_str,
              strlen(c_str));
}

}  // namespace util
}  // namespace atlas
