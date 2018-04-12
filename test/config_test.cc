#include "../util/config.h"
#include <gtest/gtest.h>
#include <rapidjson/document.h>
#include <cstdlib>
#include <fmt/format.h>

using atlas::util::Config;
using atlas::util::EndpointConfig;
using atlas::util::FeaturesConfig;
using atlas::util::HttpConfig;
using atlas::util::LogConfig;
using atlas::util::intern_str;

TEST(Config, Default) {
  auto cfg = Config();
  EXPECT_TRUE(cfg.IsMainEnabled());
}

TEST(Config, SetNotify) {
  auto cfg = Config();
  cfg.SetNotify(false);
  EXPECT_FALSE(cfg.ShouldNotifyAlertServer());
  cfg.SetNotify(true);
  EXPECT_TRUE(cfg.ShouldNotifyAlertServer());
}

TEST(Config, ParseConfig) {
  const char* disabledMain = "{\"publishEnabled\": false}";
  rapidjson::Document document;
  document.Parse(disabledMain);
  auto cfg = Config::FromJson(document, Config());
  EXPECT_FALSE(cfg->IsMainEnabled());
}

TEST(LogConfig, ParseAndCompose) {
  const char* cfgStr = R"json(
      {"logVerbosity": 1,
       "logMaxSize": 1000,
       "logMaxFiles": 42,
       "dumpMetrics": true,
       "dumpSubscriptions": true
      })json";
  rapidjson::Document document;
  document.Parse(cfgStr);

  auto check_cfg = [](const LogConfig& cfg) {
    EXPECT_EQ(cfg.verbosity, 1);
    EXPECT_EQ(cfg.max_files, 42);
    EXPECT_EQ(cfg.max_size, 1000);
    EXPECT_TRUE(cfg.dump_subscriptions);
    EXPECT_TRUE(cfg.dump_metrics);
  };

  auto cfg = LogConfig::FromJson(document, LogConfig());
  check_cfg(cfg);

  // check that we expose and parse the right subconfig
  auto c = Config::FromJson(document, Config());
  auto cfg2 = c->LogConfiguration();
  check_cfg(cfg2);
}

TEST(HttpConfig, ParseAndCompose) {
  const char* cfgStr = R"json(
      {"connectTimeout": 10,
       "readTimeout": 11,
       "sendInParallel": true,
       "batchSize": 42
      })json";
  rapidjson::Document document;
  document.Parse(cfgStr);
  auto cfg = HttpConfig::FromJson(document, HttpConfig());
  auto check_cfg = [](const HttpConfig& cfg) {
    EXPECT_EQ(cfg.connect_timeout, 10);
    EXPECT_EQ(cfg.read_timeout, 11);
    EXPECT_EQ(cfg.batch_size, 42);
    EXPECT_TRUE(cfg.send_in_parallel);
  };

  check_cfg(cfg);
  // check that we expose and parse the right subconfig
  auto c = Config::FromJson(document, Config());
  auto cfg2 = c->HttpConfiguration();
  check_cfg(cfg2);
}

TEST(EndpointConfig, ParseAndCompose) {
  setenv("EndpointConfigTest", "abcdef", 1);
  const char* cfgStr = R"json(
      {"evaluateUrl": "http://foo.$EndpointConfigTest.bar/eval",
       "subscriptionsUrl": "http://foo.$EndpointConfigTest.bar/subs",
       "publishUrl": "http://foo.$EndpointConfigTest.bar/pub",
       "checkClusterUrl": "http://foo.$EndpointConfigTest.bar/check"
      })json";
  rapidjson::Document document;
  document.Parse(cfgStr);
  auto cfg = EndpointConfig::FromJson(document, EndpointConfig());
  auto check_cfg = [](const EndpointConfig& cfg) {
    EXPECT_EQ(cfg.evaluate, "http://foo.abcdef.bar/eval");
    EXPECT_EQ(cfg.subscriptions, "http://foo.abcdef.bar/subs");
    EXPECT_EQ(cfg.publish, "http://foo.abcdef.bar/pub");
    EXPECT_EQ(cfg.check_cluster, "http://foo.abcdef.bar/check");
  };
  check_cfg(cfg);

  // check that we expose and parse the right subconfig
  auto c = Config::FromJson(document, Config());
  auto cfg2 = c->EndpointConfiguration();
  check_cfg(cfg2);
}

TEST(Features, Parse) {
  const char* cfgStr = R"json(
      {"validateMetrics": false,
       "notifyAlertServer": false,
       "publishConfig": ["class,:has,:all", ":true,(,nf.node,),:drop-tags"],
       "forceStart": true,
       "publishEnabled": true,
       "subscriptionsEnabled": true,
       "subscriptionsRefreshMillis": 1234
      })json";
  rapidjson::Document document;
  document.Parse(cfgStr);
  auto cfg = FeaturesConfig::FromJson(document, FeaturesConfig());
  EXPECT_FALSE(cfg.validate);
  EXPECT_FALSE(cfg.notify_alert_server);
  EXPECT_TRUE(cfg.force_start);
  EXPECT_TRUE(cfg.main);
  EXPECT_TRUE(cfg.subscriptions);
  EXPECT_EQ(cfg.subscription_refresh_ms, 1234);
  auto expected_pub_config = std::vector<std::string>{
      "class,:has,:all", ":true,(,nf.node,),:drop-tags"};
  EXPECT_EQ(cfg.publish_config, expected_pub_config);

  auto cfg2 = Config::FromJson(document, Config());
  EXPECT_FALSE(cfg2->ShouldValidateMetrics());
  EXPECT_FALSE(cfg2->ShouldNotifyAlertServer());
  EXPECT_EQ(cfg2->PublishConfig(), expected_pub_config);
  EXPECT_TRUE(cfg2->IsMainEnabled());
  EXPECT_TRUE(cfg2->AreSubsEnabled());
  EXPECT_EQ(cfg2->SubRefreshMillis(), 1234);
}

TEST(Config, ToString) {
  const char* sample_cfg = R"json(
      {"publishEnabled": false,
       "subscriptionsEnabled": true,
       "subscriptionsRefreshMillis": 42
      })json";
  rapidjson::Document document;
  document.Parse(sample_cfg);

  auto cfg = Config::FromJson(document, Config());
  auto str = cfg->ToString();
  constexpr auto not_found = std::string::npos;
  EXPECT_TRUE(str.find("mainEnabled=false") != not_found);
  EXPECT_TRUE(str.find("subsEnabled=true") != not_found);
  EXPECT_TRUE(str.find("subsRefresh=42") != not_found);
}

TEST(Config, LoggingDirectory) {
  auto cfg = Config();
  // running tests we set the logger to stderr, so it should be empty.
  EXPECT_TRUE(cfg.LoggingDirectory().empty());
}

TEST(Config, AddCommonTag) {
  auto cfg = Config();
  auto orig_common_tags = cfg.CommonTags();

  atlas::meter::Tags new_tags;
  new_tags.add("foo", "bar");
  cfg.AddCommonTags(new_tags);

  auto new_common_tags = cfg.CommonTags();
  auto new_key_ref = intern_str("foo");
  auto new_val_ref = intern_str("bar");
  EXPECT_EQ(new_val_ref, new_common_tags.at(new_key_ref));
  EXPECT_EQ(orig_common_tags.size() + 1, new_common_tags.size());
}
