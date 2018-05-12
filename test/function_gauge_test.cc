
#include "../meter/function_gauge.h"
#include "../meter/manual_clock.h"
#include "test_registry.h"
#include <gtest/gtest.h>

using atlas::meter::FunctionGauge;
using atlas::meter::Id;
using atlas::meter::kEmptyTags;
using atlas::meter::ManualClock;
using atlas::meter::Meter;

static ManualClock manual_clock;
static constexpr auto kNAN = std::numeric_limits<double>::quiet_NaN();

void expect_gauge_value(const Meter& m, double expected) {
  auto measures = m.Measure();
  EXPECT_EQ(measures.size(), 1);
  auto value = measures.at(0).value;
  if (std::isnan(expected)) {
    EXPECT_TRUE(std::isnan(value));
  } else {
    EXPECT_DOUBLE_EQ(value, expected);
  }
}

TEST(FunctionGauge, Init) {
  auto id = std::make_shared<Id>("foo", kEmptyTags);
  auto n = std::make_shared<int64_t>(0);
  auto f = [](int64_t v) { return v * 2.0; };
  FunctionGauge<int64_t> fg{id, manual_clock, n, f};

  EXPECT_FALSE(fg.HasExpired());
  auto ms = fg.Measure();
}

TEST(FunctionGauge, Value) {
  auto id = std::make_shared<Id>("foo", kEmptyTags);
  auto n = std::make_shared<int64_t>(21);
  auto f = [](int64_t v) { return v * 2.0; };
  FunctionGauge<int64_t> fg{id, manual_clock, n, f};

  fg.Update();
  EXPECT_DOUBLE_EQ(fg.Value(), 42.0);
}

TEST(FunctionGauge, Expiration) {
  auto id = std::make_shared<Id>("foo", kEmptyTags);
  auto n = std::make_shared<int64_t>(21);
  auto f = [](int64_t v) { return v * 2.0; };
  FunctionGauge<int64_t> fg{id, manual_clock, n, f};

  EXPECT_FALSE(fg.HasExpired());  // valid pointer
  fg.Update();
  expect_gauge_value(fg, 42.0);

  n.reset();
  EXPECT_TRUE(fg.HasExpired());  // pointer is gone

  fg.Update();
  expect_gauge_value(fg, kNAN);
}