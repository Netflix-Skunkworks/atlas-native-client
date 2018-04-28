#include "../meter/manual_clock.h"
#include "../meter/default_gauge.h"
#include <cmath>
#include <gtest/gtest.h>

using namespace atlas::meter;

static ManualClock manual_clock;

static std::unique_ptr<DefaultGauge> newGauge() {
  static auto id = std::make_shared<Id>("foo", kEmptyTags);
  return std::make_unique<DefaultGauge>(id, manual_clock);
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
