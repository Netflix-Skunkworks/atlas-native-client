#include <fstream>
#include <gtest/gtest.h>

#include "../meter/subscription_format.h"
#include "../meter/subscription_manager.h"
#include "../util/json.h"
#include "../util/logger.h"
#include "../util/config.h"

using atlas::util::Logger;

namespace atlas {
namespace meter {
SubscriptionResults generate_sub_results(
    const interpreter::Evaluator& evaluator, const Subscriptions& subs,
    const interpreter::TagsValuePairs& pairs);

ParsedSubscriptions ParseSubscriptions(char* subs_buffer);
rapidjson::Document MeasurementsToJson(
    int64_t now_millis,
    const interpreter::TagsValuePairs::const_iterator& first,
    const interpreter::TagsValuePairs::const_iterator& last, bool validate,
    int64_t* added);

rapidjson::Document SubResultsToJson(
    int64_t now_millis, const SubscriptionResults::const_iterator& first,
    const SubscriptionResults::const_iterator& last);
}  // namespace meter
}  // namespace atlas

using namespace atlas::meter;
using atlas::util::kMainFrequencyMillis;
using atlas::util::kMainMultiple;
TEST(SubscriptionsManager, ParseSubs) {
  std::ifstream one_sub("./resources/subs1.json");
  std::stringstream buffer;
  buffer << one_sub.rdbuf();

  auto subs = ParseSubscriptions(const_cast<char*>(buffer.str().c_str()));
  auto expected = Subscriptions{
      Subscription{"So3yA1c1xN_vzZASUQdORBqd9hM", kMainFrequencyMillis,
                   "nf.cluster,skan-test,:eq,name,foometric,:eq,:and,:sum"},
      Subscription{"tphgPWqODm0ZUhgUCwj23lpEs1o", kMainFrequencyMillis,
                   "nf.cluster,skan,:re,name,foometric,:eq,:and,:sum"}};
  EXPECT_EQ(subs[kMainMultiple - 1].size(), 2);
  EXPECT_EQ(subs[kMainMultiple - 1], expected);
}

TEST(SubscriptionsManager, ParseLotsOfSubs) {
  std::ifstream one_sub("./resources/many-subs.json");
  std::stringstream buffer;
  buffer << one_sub.rdbuf();

  Logger()->set_level(spdlog::level::info);
  // this is very spammy since it'll output all the subscriptions
  auto subs = ParseSubscriptions(const_cast<char*>(buffer.str().c_str()));
  Logger()->set_level(spdlog::level::debug);
  EXPECT_EQ(subs[kMainMultiple - 1].size(), 3665);
}

static std::string json_to_str(const rapidjson::Document& json) {
  rapidjson::StringBuffer buffer;
  auto c_str = atlas::util::JsonGetString(buffer, json);
  return std::string{c_str};
}

static void expect_eq_json(rapidjson::Document& expected,
                           rapidjson::Document& actual) {
  EXPECT_EQ(expected, actual) << "expected: " << json_to_str(expected)
                              << ", actual: " << json_to_str(actual);
}

TEST(SubscriptionManager, MeasurementsToJsonInvalid) {
  using namespace atlas::interpreter;
  using namespace rapidjson;

  Tags t1{{"name", "name1"}, {"k1", "v1"}, {"k2", ""}};
  Tags t2{{"name", "name2"}, {"", "v1"}, {"k2", "v2.0"}};
  Tags t3{{"k1", "v1"}, {"k2", "v2.1"}};

  auto m1 = TagsValuePair::of(std::move(t1), 1.1);
  auto m2 = TagsValuePair::of(std::move(t2), 2.2);
  auto m3 = TagsValuePair::of(std::move(t3), 3.3);
  TagsValuePairs ms{std::move(m1), std::move(m2), std::move(m3)};

  int64_t added;
  auto json =
      atlas::meter::MeasurementsToJson(1, ms.begin(), ms.end(), true, &added);
  EXPECT_EQ(added, 0);
}

TEST(SubscriptionManager, MeasurementsToJson) {
  using namespace atlas::interpreter;
  using namespace rapidjson;

  Tags t1{{"name", "name1"}, {"k1", "v1"}, {"k2", "v2"}};
  Tags t2{{"name", "name2"}, {"k1", "v1"}, {"k2", "v2.0"}};
  Tags t3{{"name", "name3"}, {"k1", "v1"}, {"k2", "v2.1"}};

  auto m1 = TagsValuePair::of(std::move(t1), 1.1);
  auto m2 = TagsValuePair::of(std::move(t2), 2.2);
  auto m3 = TagsValuePair::of(std::move(t3), 3.3);
  TagsValuePairs ms{std::move(m1), std::move(m2), std::move(m3)};

  int64_t added;
  auto json =
      atlas::meter::MeasurementsToJson(1, ms.begin(), ms.end(), true, &added);
  EXPECT_EQ(added, 3);

  const char* expected =
      "{\"tags\":{},"
      "\"metrics\":[{\"tags\":{"
      "\"k1\":\"v1\",\"k2\":\"v2\",\"name\":\"name1\"},\"start\":1,\"value\":1."
      "1},"
      "{\"tags\":{\"k1\":\"v1\",\"k2\":\"v2.0\",\"name\":\"name2\"},"
      "\"start\":1,\"value\":2.2},{\"tags\":{\"k1\":\"v1\","
      "\"k2\":\"v2.1\",\"name\":\"name3\"},\"start\":1,\"value\":3.3}]}";

  Document expected_json;
  expected_json.Parse<kParseCommentsFlag | kParseNanAndInfFlag>(expected);
  expect_eq_json(expected_json, json);
}

TEST(SubscriptionManager, SubResToJson) {
  using namespace atlas::interpreter;
  using namespace rapidjson;

  SubscriptionMetric e1{"id1", Tags{{"name", "n1"}, {"k1", "v1"}}, 42.0};
  SubscriptionMetric e2{"id1", Tags{{"name", "n1"}, {"k1", "v2"}}, 24.0};
  SubscriptionResults sub_results{e1, e2};

  auto json = SubResultsToJson(123, sub_results.begin(), sub_results.end());
  const char* expected =
      "{\"timestamp\":123,\"metrics\":[{\"id\":\"id1\",\"tags\":{\"k1\":\"v1\","
      "\"name\":\"n1\"},\"value\":42.0},{\"id\":\"id1\",\"tags\":{\"k1\":"
      "\"v2\",\"name\":\"n1\"},\"value\":24.0}]}";

  Document expected_json;
  expected_json.Parse(expected);
  expect_eq_json(expected_json, json);
}

TEST(SubscriptionManager, generate_subs_results) {
  using namespace atlas::interpreter;

  Evaluator evaluator;
  Subscriptions subs;
  subs.emplace_back(Subscription{"id", 60000, ":true,:all"});
  subs.emplace_back(
      Subscription{"id2", 60000, "name,name1,:eq,:sum,(,k2,),:by"});
  subs.emplace_back(Subscription{"id3", 60000, "name,name3,:eq,:sum"});
  subs.emplace_back(Subscription{"id3", 60000, "name,name4,:eq,:sum"});
  Tags t1{{"name", "name1"}, {"k1", "v1"}, {"k2", "v2"}};
  Tags t2{{"name", "name2"}, {"k1", "v1"}, {"k2", "v2.0"}};
  Tags t3{{"name", "name3"}, {"k1", "v1"}, {"k2", "v2.1"}};

  auto m1 = TagsValuePair::of(std::move(t1), 1.1);
  auto m2 = TagsValuePair::of(std::move(t2), 2.2);
  auto m3 = TagsValuePair::of(std::move(t3), 3.3);
  TagsValuePairs ms{std::move(m1), std::move(m2), std::move(m3)};

  auto g = generate_sub_results(evaluator, subs, ms);
  for (const auto& sr : g) {
    Logger()->info("Got {}", sr);
  }
  EXPECT_EQ(g.size(), 5);
}