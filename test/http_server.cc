#include "http_server.h"
#include "../util/logger.h"
#include "../util/gzip.h"
#include <fmt/format.h>
#include <thread>
#include <sys/socket.h>
#include <gtest/gtest.h>
#include <fstream>
#include <netinet/in.h>

using atlas::util::gzip_compress;
using atlas::util::Logger;

static std::string compress_file(const char* file_name) {
  std::ifstream to_compress(file_name);
  std::stringstream buffer;
  buffer << to_compress.rdbuf();
  auto raw = buffer.str();
  size_t buf_len = 1024 * 1024;
  auto buf = std::unique_ptr<char[]>(new char[buf_len]);
  auto gzip_res = gzip_compress(buf.get(), &buf_len, raw.c_str(), raw.length());
  if (gzip_res != Z_OK) {
    Logger()->error("Unable to compress {}: gzip err {}", file_name, gzip_res);
    return "";
  }
  return std::string{buf.get(), buf_len};
}

http_server::http_server() noexcept {
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

http_server::~http_server() {
  std::lock_guard<std::mutex> guard(requests_mutex_);
  if (sockfd_ >= 0) {
    close(sockfd_);
  }
}

void http_server::start() noexcept {
  struct sockaddr_in serv_addr;
  sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
  ASSERT_TRUE(sockfd_ >= 0);

  bzero(&serv_addr, sizeof serv_addr);
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = 0;

  auto sockfd = sockfd_;
  ASSERT_TRUE(bind(sockfd, (sockaddr*)&serv_addr, sizeof serv_addr) >= 0);
  ASSERT_TRUE(listen(sockfd, 32) == 0);

  socklen_t serv_len = sizeof serv_addr;
  ASSERT_TRUE(getsockname(sockfd, (sockaddr*)&serv_addr, &serv_len) >= 0);
  port_ = ntohs(serv_addr.sin_port);

  std::thread acceptor(&http_server::accept_loop, this);
  acceptor.detach();
}

std::string http_server::Request::get_header(const std::string& name) const {
  auto lower = atlas::util::ToLowerCopy(name);
  auto it = headers_.find(lower);
  if (it == headers_.end()) {
    return "";
  }
  return it->second;
}

const std::vector<http_server::Request>& http_server::get_requests() const {
  std::lock_guard<std::mutex> guard(requests_mutex_);
  return requests_;
};

static void get_line(int client, char* buf, size_t size) {
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

void http_server::accept_request(int client) {
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
    requests_.emplace_back(method, path, headers, content_len, std::move(body));
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

void http_server::accept_loop() {
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