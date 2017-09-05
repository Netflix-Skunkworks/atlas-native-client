#include "../meter/subscription_max_gauge.h"
#include "../meter/manual_clock.h"
#include "test_registry.h"
#include <cmath>
#include <gtest/gtest.h>

using namespace atlas::meter;

static ManualClock manual_clock;

static Pollers pollers;

static TestRegistry test_registry;
static auto id = test_registry.CreateId("foo", kEmptyTags);

static std::unique_ptr<SubscriptionMaxGaugeInt> newMaxGaugeInt() {
  manual_clock.SetWall(0);
  pollers.clear();
  return std::make_unique<SubscriptionMaxGaugeInt>(id, manual_clock, pollers);
}

static std::unique_ptr<SubscriptionMaxGauge<double>> newMaxGaugeDouble() {
  manual_clock.SetWall(0);
  pollers.clear();
  return std::make_unique<SubscriptionMaxGauge<double>>(id, manual_clock,
                                                        pollers);
}

TEST(SubMaxGaugeInt, Init) {
  auto mg = newMaxGaugeInt();
  EXPECT_EQ(mg->Value(), 0);
  EXPECT_TRUE(mg->Measure().empty());
}

TEST(SubMaxGaugeInt, OnePoller) {
  auto mg = newMaxGaugeInt();
  pollers.push_back(60000);
  mg->UpdatePollers();
  auto ms = mg->MeasuresForPoller(0);
  EXPECT_EQ(1, ms.size());
  EXPECT_TRUE(std::isnan(ms[0].value));

  mg->Update(42);
  manual_clock.SetWall(60000);
  auto one = mg->MeasuresForPoller(0);

  EXPECT_EQ(1, one.size());
  EXPECT_DOUBLE_EQ(42.0, one[0].value);
}

TEST(SubMaxGaugeDouble, Init) {
  auto mg = newMaxGaugeDouble();
  EXPECT_DOUBLE_EQ(mg->Value(), 0);
  EXPECT_TRUE(mg->Measure().empty());
}

TEST(SubMaxGaugeDouble, OnePoller) {
  auto mg = newMaxGaugeDouble();
  pollers.push_back(60000);
  mg->UpdatePollers();
  auto ms = mg->MeasuresForPoller(0);
  EXPECT_DOUBLE_EQ(1, ms.size());
  EXPECT_TRUE(std::isnan(ms[0].value));

  mg->Update(42.1);
  manual_clock.SetWall(60000);
  auto one = mg->MeasuresForPoller(0);

  EXPECT_EQ(1, one.size());
  EXPECT_DOUBLE_EQ(42.1, one[0].value);
}
