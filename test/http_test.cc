#include <arpa/inet.h>
#include <atomic>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <zlib.h>

#include "../interpreter/tags_value.h"
#include "../meter/id_format.h"
#include "../util/config_manager.h"
#include "../util/gzip.h"
#include "../util/http.h"
#include "../util/logger.h"
#include "../util/strings.h"
#include "http_server.h"
#include "test_registry.h"
#include <fstream>
#include <thread>

using atlas::meter::ManualClock;
using atlas::meter::Tags;
using atlas::util::gzip_uncompress;
using atlas::util::http;
using atlas::util::HttpConfig;
using atlas::util::Logger;

static std::shared_ptr<atlas::meter::Meter> find_meter(
    atlas::meter::Registry* registry, const char* name,
    const char* status_code) {
  auto meters = registry->meters();
  auto status_ref = atlas::util::intern_str(status_code);
  for (const auto& m : meters) {
    auto meter_name = m->GetId()->Name();
    if (strcmp(meter_name, name) == 0) {
      auto status = atlas::util::intern_str("statusCode");
      auto t = m->GetId()->GetTags().at(status);
      if (t == status_ref) {
        return m;
      }
    }
  }
  return nullptr;
}

TEST(HttpTest, Post) {
  http_server server;
  server.start();

  auto port = server.get_port();
  ASSERT_TRUE(port > 0) << "Port = " << port;
  auto logger = Logger();
  logger->info("Server started on port {}", port);

  auto cfg = HttpConfig();
  ManualClock clock;
  auto registry = std::make_shared<TestRegistry>(60000, &clock);
  http client{registry, cfg};
  auto url = fmt::format("http://localhost:{}/foo", port);
  const std::string post_data = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  client.post(url, "Content-type: application/json", post_data.c_str(),
              post_data.length());

  server.stop();
  auto timer_for_req = find_meter(registry.get(), "http.req.complete", "200");
  auto expected_tags = Tags{{"client", "atlas-native-client"},
                            {"statusCode", "200"},
                            {"status", "2xx"},
                            {"mode", "http-client"},
                            {"method", "POST"}};
  EXPECT_EQ(expected_tags, timer_for_req->GetId()->GetTags());

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
  ManualClock clock;
  auto registry = std::make_shared<TestRegistry>(60000, &clock);
  http client{registry, cfg};

  auto url = fmt::format("http://localhost:{}/foo", port);
  const std::string post_data = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

  constexpr size_t kBatches = 16;
  std::vector<rapidjson::Document> batches(kBatches);
  for (auto& batch : batches) {
    batch = get_json_doc();
  }

  auto res = client.post_batches(url, batches);
  server.stop();
  auto timer_for_req = find_meter(registry.get(), "http.req.complete", "200");
  ASSERT_TRUE(timer_for_req);

  auto expected_tags = Tags{{"client", "atlas-native-client"},
                            {"statusCode", "200"},
                            {"status", "2xx"},
                            {"mode", "http-client"},
                            {"method", "POST"}};
  EXPECT_EQ(expected_tags, timer_for_req->GetId()->GetTags());

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
  ManualClock clock;
  auto registry = std::make_shared<TestRegistry>(60000, &clock);
  http client{registry, cfg};
  auto url = fmt::format("http://localhost:{}/foo", port);
  const std::string post_data = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  auto status = client.post(url, "Content-type: application/json",
                            post_data.c_str(), post_data.length());

  server.stop();
  EXPECT_EQ(status, 400);

  auto timer_for_req =
      find_meter(registry.get(), "http.req.complete", "timeout");
  auto expected_tags = Tags{{"client", "atlas-native-client"},
                            {"statusCode", "timeout"},
                            {"status", "timeout"},
                            {"mode", "http-client"},
                            {"method", "POST"}};
  EXPECT_EQ(expected_tags, timer_for_req->GetId()->GetTags());
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
  ManualClock clock;
  auto registry = std::make_shared<TestRegistry>(60000, &clock);
  http client{registry, cfg};
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

  auto timer_for_req = find_meter(registry.get(), "http.req.complete", "200");
  auto expected_tags = Tags{{"client", "atlas-native-client"},
                            {"statusCode", "200"},
                            {"status", "2xx"},
                            {"mode", "http-client"},
                            {"method", "GET"}};
  EXPECT_EQ(expected_tags, timer_for_req->GetId()->GetTags());

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

  auto timer304 = find_meter(registry.get(), "http.req.complete", "304");
  auto not_modified_tags = Tags{{"client", "atlas-native-client"},
                                {"statusCode", "304"},
                                {"status", "3xx"},
                                {"mode", "http-client"},
                                {"method", "GET"}};
  EXPECT_EQ(not_modified_tags, timer304->GetId()->GetTags());
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
  cfg.read_timeout = 60;
  ManualClock manual_clock;
  auto registry = std::make_shared<TestRegistry>(60000, &manual_clock);
  http client{registry, cfg};

  auto url = fmt::format("http://localhost:{}/compressed", port);
  std::string content;
  ASSERT_EQ(client.get(url, &content), 200);
  ASSERT_EQ(content.length(), get_file_size("./resources/subs1.json"));

  auto bigUrl = fmt::format("http://localhost:{}/compressedBig", port);
  std::string big;
  ASSERT_EQ(client.get(bigUrl, &big), 200);
  ASSERT_EQ(big.length(), get_file_size("./resources/many-subs.json"));
}
