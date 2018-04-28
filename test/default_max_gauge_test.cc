#include "../meter/default_max_gauge.h"
#include "../meter/manual_clock.h"
#include "test_registry.h"
#include <cmath>
#include <gtest/gtest.h>

using namespace atlas::meter;
using atlas::util::kMainFrequencyMillis;

static ManualClock manual_clock;

static TestRegistry test_registry;
static auto id = test_registry.CreateId("foo", kEmptyTags);

static std::unique_ptr<DefaultMaxGaugeInt> newMaxGaugeInt() {
  manual_clock.SetWall(0);
  return std::make_unique<DefaultMaxGaugeInt>(id, manual_clock,
                                              kMainFrequencyMillis);
}

static std::unique_ptr<DefaultMaxGauge<double>> newMaxGaugeDouble() {
  manual_clock.SetWall(0);
  return std::make_unique<DefaultMaxGauge<double>>(id, manual_clock,
                                                   kMainFrequencyMillis);
}

TEST(DefaultMaxGaugeInt, Init) {
  auto mg = newMaxGaugeInt();
  EXPECT_EQ(mg->Value(), 0);
}

TEST(DefaultMaxGaugeInt, OnePoller) {
  auto mg = newMaxGaugeInt();
  auto ms = mg->Measure();
  EXPECT_EQ(1, ms.size());
  EXPECT_TRUE(std::isnan(ms[0].value));

  mg->Update(42);
  manual_clock.SetWall(60000);
  auto one = mg->Measure();

  EXPECT_EQ(1, one.size());
  EXPECT_DOUBLE_EQ(42.0, one[0].value);
}

TEST(DefaultMaxGaugeDouble, Init) {
  auto mg = newMaxGaugeDouble();
  EXPECT_DOUBLE_EQ(mg->Value(), 0);
}

TEST(DefaultMaxGaugeDouble, OnePoller) {
  auto mg = newMaxGaugeDouble();
  auto ms = mg->Measure();
  EXPECT_DOUBLE_EQ(1, ms.size());
  EXPECT_TRUE(std::isnan(ms[0].value));

  mg->Update(42.1);
  manual_clock.SetWall(60000);
  auto one = mg->Measure();

  EXPECT_EQ(1, one.size());
  EXPECT_DOUBLE_EQ(42.1, one[0].value);
}
