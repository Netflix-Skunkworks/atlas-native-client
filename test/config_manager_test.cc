#include "../util/config_manager.h"
#include <gtest/gtest.h>
#include <thread>

using atlas::util::ConfigManager;

TEST(ConfigManager, Init) {
  ConfigManager cm("./resources/config.json", 1);
  auto cfg = cm.GetConfig();
  EXPECT_TRUE(cfg->ShouldForceStart());
  EXPECT_FALSE(cfg->ShouldNotifyAlertServer());

  const auto& endpoints = cfg->EndpointConfiguration();
  EXPECT_EQ(endpoints.publish, "http://atlas.example.com/pub");
  EXPECT_EQ(endpoints.evaluate, "http://atlas.example.com/eval");
  EXPECT_EQ(endpoints.subscriptions, "http://atlas.example.com/subs");
  EXPECT_EQ(endpoints.check_cluster, "http://atlas.example.com/check");
}
