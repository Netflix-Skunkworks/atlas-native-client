#include "../meter/manual_clock.h"
#include "../meter/subscription_gauge.h"
#include "test_registry.h"
#include <cmath>
#include <gtest/gtest.h>

using namespace atlas::meter;

static ManualClock manual_clock;

static TestRegistry test_registry;
static auto id = test_registry.CreateId("foo", kEmptyTags);

static std::unique_ptr<SubscriptionGauge> newGauge() {
  return std::make_unique<SubscriptionGauge>(id, manual_clock);
}

TEST(GaugeTest, Value) {
  auto g = newGauge();
  manual_clock.SetWall(1);
  EXPECT_TRUE(std::isnan(g->Value()));
  g->Update(42.0);
  EXPECT_DOUBLE_EQ(g->Value(), 42.0);
}

TEST(GaugeTest, Expiration) {
  auto g = newGauge();
  manual_clock.SetWall(1);
  g->Update(42.0);
  EXPECT_FALSE(g->HasExpired());
  manual_clock.SetWall(MAX_IDLE_TIME + 1000);
  EXPECT_TRUE(g->HasExpired());
}
