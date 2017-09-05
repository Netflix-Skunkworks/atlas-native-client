#include "../meter/subscription_counter.h"
#include "../meter/manual_clock.h"
#include "test_registry.h"
#include <gtest/gtest.h>

using namespace atlas::meter;

static ManualClock manual_clock;

static Pollers pollers;

static TestRegistry test_registry;
static auto id = test_registry.CreateId("foo", kEmptyTags);

static std::unique_ptr<SubscriptionCounter> newCounter() {
  pollers.clear();
  return std::make_unique<SubscriptionCounter>(id, manual_clock, pollers);
}

TEST(SubCounterTest, Init) {
  auto counter = newCounter();
  EXPECT_EQ(0, counter->Count()) << "A new counter should report a value of 0";
}

TEST(SubCounterTest, Increment) {
  auto counter = newCounter();
  counter->Increment();
  EXPECT_EQ(1, counter->Count()) << "One increment should be 1";
  counter->Increment();
  counter->Increment();
  EXPECT_EQ(3, counter->Count()) << "3 increments should be 3";
}

TEST(SubCounterTest, Add) {
  auto counter = newCounter();
  counter->Add(42);
  EXPECT_EQ(42, counter->Count()) << "Adding 42 to 0";
}

TEST(SubCounterTest, Measure) {
  auto counter = newCounter();
  auto expected_id = *(Id("foo").WithTag(statistic::count));
  counter->Add(42);
  const Measurements& empty_ms = counter->Measure();
  EXPECT_EQ(0, empty_ms.size());

  // simulate subscription-manager updating subscription
  pollers.push_back(60000);
  counter->UpdatePollers();

  // now we have one valid step_long
  counter->Increment();
  manual_clock.SetWall(60000);
  const Measurements& ms = counter->Measure();  // same a measure_for_poller(0);
  EXPECT_EQ(1, ms.size());

  for (auto& m : ms) {
    EXPECT_EQ(expected_id, *(m.id));
    EXPECT_EQ(60000, m.timestamp);
    EXPECT_DOUBLE_EQ(1.0 / 60.0, m.value);
  }

  // add a new subscription at a 10s interval
  pollers.push_back(10000);
  counter->UpdatePollers();

  // now we have two valid step_longs
  manual_clock.SetWall(110000);
  counter->Add(2);
  manual_clock.SetWall(120000);
  const Measurements& ms_60 = counter->MeasuresForPoller(0);
  EXPECT_EQ(1, ms_60.size());

  for (auto& m : ms_60) {
    EXPECT_EQ(expected_id, *(m.id));
    EXPECT_EQ(120000, m.timestamp);
    EXPECT_DOUBLE_EQ(2.0 / 60.0, m.value);
  }

  const Measurements& ms_10 = counter->MeasuresForPoller(1);
  EXPECT_EQ(1, ms_10.size());

  for (auto& m : ms_10) {
    EXPECT_EQ(expected_id, *(m.id));
    EXPECT_EQ(120000, m.timestamp);
    EXPECT_DOUBLE_EQ(2.0 / 10.0, m.value);
  }
}
