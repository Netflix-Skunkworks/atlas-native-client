#include <arpa/inet.h>
#include <atomic>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <zlib.h>

#include "../util/gzip.h"
#include "../util/http.h"
#include "../util/logger.h"
#include "../util/strings.h"
#include <thread>

using atlas::util::Logger;
using atlas::util::ToLowerCopy;

const static std::string EMPTY_STRING = "";

class http_server {
 public:
  http_server() = default;
  http_server(const http_server&) = delete;
  http_server(http_server&&) = delete;
  http_server& operator=(const http_server&) = delete;
  http_server& operator=(http_server&&) = delete;

  ~http_server() {
    if (sockfd_ >= 0) {
      close(sockfd_);
    }
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
    ASSERT_TRUE(listen(sockfd_, 5) == 0);

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
    std::map<std::string, std::string> headers_;
    size_t size_;
    std::unique_ptr<char[]> body_;
  };

  const std::vector<Request>& get_requests() const { return requests_; };

 private:
  int sockfd_ = -1;
  int port_ = 0;
  std::atomic<bool> is_done{false};
  std::vector<Request> requests_;
  int accept_sleep_ = 0;
  int read_sleep_ = 0;

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

  static constexpr const char* const response =
      "HTTP/1.1 200 OK\nContent-Length: 0\nServer: atlas-tests\n";
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
    int content_len = stoi(headers["content-length"]);
    auto body = std::unique_ptr<char[]>(new char[content_len + 1]);

    char* p = body.get();
    auto n = content_len;
    while (n > 0) {
      auto bytes_read = read(client, p, (size_t)n);
      n -= bytes_read;
      p += bytes_read;
    }
    *p = '\0';

    if (read_sleep_ > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(read_sleep_));
    }
    auto left_to_write = strlen(response);
    auto resp_ptr = response;
    while (left_to_write > 0) {
      auto written = write(client, resp_ptr, strlen(response));
      resp_ptr += written;
      left_to_write -= written;
    }

    requests_.emplace_back(method, path, headers, content_len, std::move(body));
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

  http client{1, 1};
  std::ostringstream os;
  os << "http://localhost:" << port << "/foo";
  auto url = os.str();
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
  auto res = atlas::util::gzip_uncompress((Bytef*)dest, &dest_len,
                                          (const Bytef*)src, src_len);
  ASSERT_EQ(res, Z_OK);

  std::string body_str{dest, dest_len};
  EXPECT_EQ(post_data, body_str);
}

TEST(HttpTest, Timeout) {
  using atlas::util::http;
  http_server server;
  server.set_accept_sleep(1500);
  //  server.set_read_sleep(0);
  server.start();

  auto port = server.get_port();
  ASSERT_TRUE(port > 0) << "Port = " << port;
  auto logger = Logger();
  logger->info("Server started on port {}", port);

  http client{1, 1};
  std::ostringstream os;
  os << "http://localhost:" << port << "/foo";
  auto url = os.str();
  const std::string post_data = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  client.post(url, "Content-type: application/json", post_data.c_str(),
              post_data.length());

  server.stop();
}
