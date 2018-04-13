#include <arpa/inet.h>
#include <atomic>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <zlib.h>

#include "../util/config_manager.h"
#include "../util/gzip.h"
#include "../util/http.h"
#include "../util/logger.h"
#include "../util/strings.h"
#include "../interpreter/tags_value.h"
#include <thread>
#include <fstream>

using atlas::util::HttpConfig;
using atlas::util::Logger;
using atlas::util::ToLowerCopy;
using atlas::util::gzip_compress;
using atlas::util::gzip_uncompress;

const static std::string EMPTY_STRING = "";

class http_server {
 public:
  http_server() {
    path_response_["/foo"] =
        "HTTP/1.1 200 OK\nContent-Length: 0\nServer: atlas-tests\n";
    path_response_["/get"] =
        "HTTP/1.0 200 OK\nContent-Length: 10\nServer: atlas-tests\nEtag: "
        "1234\n\n123456790";
    path_response_["/get304"] =
        "HTTP/1.0 304 OK\nContent-Length: 0\nServer: atlas-tests\nEtag: "
        "1234\n\n";

    std::string small = compress_file("./resources/subs1.json");
    path_response_["/compressed"] =
        fmt::format(
            "HTTP/1.0 200 OK\nContent-Encoding: gzip\nContent-Length: {}\n\n",
            small.length()) +
        small;
    std::string big = compress_file("./resources/many-subs.json");
    path_response_["/compressedBig"] =
        fmt::format(
            "HTTP/1.0 200 OK\nContent-Encoding: gzip\nContent-Length: {}\n\n",
            big.length()) +
        big;
  }
  http_server(const http_server&) = delete;
  http_server(http_server&&) = delete;
  http_server& operator=(const http_server&) = delete;
  http_server& operator=(http_server&&) = delete;

  ~http_server() {
    if (sockfd_ >= 0) {
      close(sockfd_);
    }
  }

  std::string compress_file(const char* file_name) {
    std::ifstream to_compress(file_name);
    std::stringstream buffer;
    buffer << to_compress.rdbuf();
    auto raw = buffer.str();
    size_t buf_len = 1024 * 1024;
    auto buf = std::unique_ptr<char[]>(new char[buf_len]);
    auto gzip_res =
        gzip_compress(buf.get(), &buf_len, raw.c_str(), raw.length());
    if (gzip_res != Z_OK) {
      Logger()->error("Unable to compress {}: gzip err {}", file_name,
                      gzip_res);
      return "";
    }
    return std::string{buf.get(), buf_len};
  }

  void set_read_sleep(int millis) { read_sleep_ = millis; }

  void set_accept_sleep(int millis) { accept_sleep_ = millis; }

  void start() {
    struct sockaddr_in serv_addr;
    sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_TRUE(sockfd_ >= 0);

    bzero(&serv_addr, sizeof serv_addr);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = 0;

    ASSERT_TRUE(bind(sockfd_, (sockaddr*)&serv_addr, sizeof serv_addr) >= 0);
    ASSERT_TRUE(listen(sockfd_, 32) == 0);

    socklen_t serv_len = sizeof serv_addr;
    ASSERT_TRUE(getsockname(sockfd_, (sockaddr*)&serv_addr, &serv_len) >= 0);
    port_ = ntohs(serv_addr.sin_port);

    std::thread acceptor([this] { accept_loop(); });
    acceptor.detach();
  }

  int get_port() const { return port_; };
  void stop() { is_done = true; }

  class Request {
   public:
    Request() : size_(0), body_(nullptr) {}
    Request(std::string method, std::string path,
            std::map<std::string, std::string> headers, size_t size,
            std::unique_ptr<char[]>&& body)
        : method_(std::move(method)),
          path_(std::move(path)),
          headers_(std::move(headers)),
          size_(size),
          body_(std::move(body)) {}

    size_t size() const { return size_; }
    const char* body() const { return body_.get(); }
    const std::string& get_header(const std::string& name) const {
      auto lower = ToLowerCopy(name);
      auto it = headers_.find(lower);
      if (it == headers_.end()) {
        return EMPTY_STRING;
      }
      return it->second;
    }
    const std::string& method() const { return method_; }
    const std::string& path() const { return path_; }

   private:
    std::string method_;
    std::string path_;
    std::map<std::string, std::string> headers_{};
    size_t size_;
    std::unique_ptr<char[]> body_{};
  };

  const std::vector<Request>& get_requests() const {
    std::lock_guard<std::mutex> guard(requests_mutex_);
    return requests_;
  };

 private:
  int sockfd_ = -1;
  int port_ = 0;
  std::atomic<bool> is_done{false};
  mutable std::mutex requests_mutex_{};
  std::vector<Request> requests_;
  int accept_sleep_ = 0;
  int read_sleep_ = 0;
  std::map<std::string, std::string> path_response_;

  void get_line(int client, char* buf, size_t size) {
    assert(size > 0);
    size_t i = 0;
    char c = '\0';

    while ((i < size - 1) && (c != '\n')) {
      auto n = recv(client, &c, 1, 0);
      if (n > 0) {
        if (c == '\r') {
          n = recv(client, &c, 1, MSG_PEEK);
          if (n > 0 && c == '\n') {
            recv(client, &c, 1, 0);
          } else {
            c = '\n';
          }
        }
        buf[i++] = c;
      } else {
        c = '\n';
      }
    }
    buf[i] = '\0';
  }

  void accept_request(int client) {
    using namespace std;

    char buf[4096];
    get_line(client, buf, sizeof buf);

    char method[256];
    char path[1024];
    size_t i = 0, j = 0;
    while (!isspace(buf[i]) && i < (sizeof method - 1)) {
      method[i++] = buf[j++];
    }
    method[i] = '\0';
    while (isspace(buf[j]) && j < sizeof buf) {
      ++j;
    }

    i = 0;
    while (!isspace(buf[j]) && i < (sizeof path - 1) && j < sizeof buf) {
      path[i++] = buf[j++];
    }
    path[i] = '\0';

    // do headers
    map<string, string> headers;
    for (;;) {
      get_line(client, buf, sizeof buf);
      if (strlen(buf) <= 1) {
        break;
      }

      i = 0;
      while (buf[i] != ':' && i < (sizeof buf - 1)) {
        buf[i] = (char)tolower(buf[i]);
        ++i;
      }
      buf[i++] = '\0';

      while (isspace(buf[i]) && i < sizeof buf) {
        ++i;
      }

      std::string header_name = buf;
      j = i;
      while (buf[i] != '\n' && i < (sizeof buf - 1)) {
        ++i;
      }
      buf[i] = '\0';
      std::string header_value = &buf[j];
      headers[header_name] = header_value;
    }

    // do the body
    auto len = headers["content-length"];
    int content_len = len.empty() ? 0 : stoi(len);

    auto body = std::unique_ptr<char[]>(new char[content_len + 1]);

    char* p = body.get();
    auto n = content_len;
    while (n > 0) {
      auto bytes_read = read(client, p, (size_t)n);
      n -= bytes_read;
      p += bytes_read;
    }
    *p = '\0';
    Logger()->debug("Adding request {} {}", method, path);
    {
      std::lock_guard<std::mutex> guard(requests_mutex_);
      requests_.emplace_back(method, path, headers, content_len,
                             std::move(body));
    }

    if (read_sleep_ > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(read_sleep_));
    }
    auto response = path_response_[path];
    auto left_to_write = response.length() + 1;
    auto resp_ptr = response.c_str();
    while (left_to_write > 0) {
      auto written = write(client, resp_ptr, left_to_write);
      resp_ptr += written;
      left_to_write -= written;
    }
  }

  void accept_loop() {
    while (!is_done) {
      struct sockaddr_in cli_addr;
      socklen_t cli_len = sizeof(cli_addr);
      if (accept_sleep_ > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(accept_sleep_));
      }
      int client_socket = accept(sockfd_, (sockaddr*)&cli_addr, &cli_len);
      if (client_socket >= 0) {
        accept_request(client_socket);
        close(client_socket);
      }
    }
  }
};

TEST(HttpTest, Post) {
  using atlas::util::http;

  http_server server;
  server.start();

  auto port = server.get_port();
  ASSERT_TRUE(port > 0) << "Port = " << port;
  auto logger = Logger();
  logger->info("Server started on port {}", port);

  auto cfg = HttpConfig();
  http client{cfg};
  auto url = fmt::format("http://localhost:{}/foo", port);
  const std::string post_data = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  client.post(url, "Content-type: application/json", post_data.c_str(),
              post_data.length());

  server.stop();

  const auto& requests = server.get_requests();
  EXPECT_EQ(requests.size(), 1);

  const auto& r = requests[0];
  EXPECT_EQ(r.method(), "POST");
  EXPECT_EQ(r.path(), "/foo");
  EXPECT_EQ(r.get_header("Content-Encoding"), "gzip");
  EXPECT_EQ(r.get_header("Content-Type"), "application/json");

  const auto src = r.body();
  const auto src_len = r.size();
  char dest[8192];
  size_t dest_len = sizeof dest;
  auto res = gzip_uncompress(dest, &dest_len, src, src_len);
  ASSERT_EQ(res, Z_OK);

  std::string body_str{dest, dest_len};
  EXPECT_EQ(post_data, body_str);
}

namespace atlas {
namespace meter {
rapidjson::Document MeasurementsToJson(
    int64_t now_millis,
    const interpreter::TagsValuePairs::const_iterator& first,
    const interpreter::TagsValuePairs::const_iterator& last, bool validate,
    int64_t* metrics_added);
}  // namespace meter
}  // namespace atlas

static rapidjson::Document get_json_doc() {
  using atlas::interpreter::TagsValuePair;
  using atlas::interpreter::TagsValuePairs;
  using atlas::meter::Tags;

  Tags t1{{"name", "name1"}, {"k1", "v1"}};
  Tags t2{{"name", "name1"}, {"k1", "v2"}};
  auto exp1 = TagsValuePair::of(std::move(t1), 1.0);
  auto exp2 = TagsValuePair::of(std::move(t2), 0.0);

  TagsValuePairs tag_values;
  tag_values.push_back(std::move(exp1));
  tag_values.push_back(std::move(exp2));

  int64_t n;
  return atlas::meter::MeasurementsToJson(1000, tag_values.begin(),
                                          tag_values.end(), false, &n);
}

TEST(HttpTest, PostBatches) {
  using atlas::util::http;

  http_server server;
  server.start();

  auto port = server.get_port();
  ASSERT_TRUE(port > 0) << "Port = " << port;
  auto logger = Logger();
  logger->info("Server started on port {}", port);

  auto cfg = HttpConfig();
  http client{cfg};
  auto url = fmt::format("http://localhost:{}/foo", port);
  const std::string post_data = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

  constexpr size_t kBatches = 16;
  std::vector<rapidjson::Document> batches(kBatches);
  for (auto& batch : batches) {
    batch = get_json_doc();
  }

  auto res = client.post_batches(url, batches);

  server.stop();

  const auto& requests = server.get_requests();
  EXPECT_EQ(requests.size(), kBatches);
  EXPECT_EQ(res.size(), kBatches);
  for (auto i = 0u; i < kBatches; ++i) {
    EXPECT_EQ(res[i], 200);
  }

  for (const auto& r : requests) {
    EXPECT_EQ(r.method(), "POST");
    EXPECT_EQ(r.path(), "/foo");
    EXPECT_EQ(r.get_header("Content-Encoding"), "gzip");
    EXPECT_EQ(r.get_header("Content-Type"), "application/json");
  }
}

TEST(HttpTest, Timeout) {
  using atlas::util::http;
  http_server server;
  server.set_read_sleep(1500);
  server.start();

  auto port = server.get_port();
  ASSERT_TRUE(port > 0) << "Port = " << port;
  auto logger = Logger();
  logger->info("Server started on port {}", port);

  auto cfg = HttpConfig();
  cfg.connect_timeout = 1;
  cfg.read_timeout = 1;
  http client{cfg};
  auto url = fmt::format("http://localhost:{}/foo", port);
  const std::string post_data = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  auto status = client.post(url, "Content-type: application/json",
                            post_data.c_str(), post_data.length());

  server.stop();
  EXPECT_EQ(status, 400);
}

TEST(HttpTest, ConditionalGet) {
  using atlas::util::http;
  http_server server;
  server.start();
  auto port = server.get_port();
  ASSERT_TRUE(port > 0) << "Port = " << port;
  auto logger = Logger();
  logger->info("Server started on port {}", port);

  auto cfg = HttpConfig();
  cfg.read_timeout = 60;
  http client{cfg};
  auto url = fmt::format("http://localhost:{}/get", port);

  std::string etag;
  std::string content;
  ASSERT_EQ(client.conditional_get(url, &etag, &content), 200);
  ASSERT_EQ(content.length(), 10);
  const auto& requests = server.get_requests();
  ASSERT_EQ(requests.size(), 1);
  const auto& req = requests.front();
  EXPECT_EQ(req.method(), "GET");
  EXPECT_EQ(req.path(), "/get");
  EXPECT_EQ(req.get_header("Accept-Encoding"), "gzip");
  EXPECT_EQ(req.get_header("Accept"), "*/*");
  EXPECT_EQ(etag, "1234");

  auto cond_url = fmt::format("http://localhost:{}/get304", port);
  ASSERT_EQ(client.conditional_get(cond_url, &etag, &content), 304);
  ASSERT_TRUE(content.length() > 0);
  ASSERT_EQ(server.get_requests().size(), 2);
  const auto& cond_req = server.get_requests().back();

  EXPECT_EQ(cond_req.method(), "GET");
  EXPECT_EQ(cond_req.path(), "/get304");
  EXPECT_EQ(cond_req.get_header("Accept-Encoding"), "gzip");
  EXPECT_EQ(cond_req.get_header("Accept"), "*/*");
  EXPECT_EQ(cond_req.get_header("If-None-Match"), "1234");
  EXPECT_EQ(etag, "1234");

  server.stop();
}

off_t get_file_size(const char* file) {
  struct stat st;
  auto err = stat(file, &st);
  if (err != 0) {
    Logger()->error("Can't stat {}: {}", file, strerror(err));
    return 0;
  }
  return st.st_size;
}

TEST(HttpTest, CompressedGet) {
  using atlas::util::http;
  http_server server;
  server.start();
  auto port = server.get_port();
  ASSERT_TRUE(port > 0) << "Port = " << port;
  auto logger = Logger();
  logger->info("Server started on port {}", port);

  auto cfg = HttpConfig();
  http client{cfg};
  cfg.read_timeout = 60;
  auto url = fmt::format("http://localhost:{}/compressed", port);
  std::string content;
  ASSERT_EQ(client.get(url, &content), 200);
  ASSERT_EQ(content.length(), get_file_size("./resources/subs1.json"));

  auto bigUrl = fmt::format("http://localhost:{}/compressedBig", port);
  std::string big;
  ASSERT_EQ(client.get(bigUrl, &big), 200);
  ASSERT_EQ(big.length(), get_file_size("./resources/many-subs.json"));
}
