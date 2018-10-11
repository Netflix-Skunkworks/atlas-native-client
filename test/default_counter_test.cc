#include "../meter/default_counter.h"
#include "../meter/manual_clock.h"
#include "../util/config.h"
#include "test_registry.h"
#include <gtest/gtest.h>

using namespace atlas::meter;
using atlas::util::kMainFrequencyMillis;

static ManualClock manual_clock;

static std::unique_ptr<DefaultCounter> newCounter() {
  static auto id = std::make_shared<Id>("foo", kEmptyTags);
  return std::make_unique<DefaultCounter>(id, manual_clock,
                                          kMainFrequencyMillis);
}

TEST(DefaultCounterTest, Init) {
  auto counter = newCounter();
  EXPECT_EQ(0, counter->Count()) << "A new counter should report a value of 0";
}

TEST(DefaultCounterTest, Increment) {
  auto counter = newCounter();
  counter->Increment();
  EXPECT_EQ(1, counter->Count()) << "One increment should be 1";
  counter->Increment();
  counter->Increment();
  EXPECT_EQ(3, counter->Count()) << "3 increments should be 3";
}

TEST(DefaultCounterTest, Add) {
  auto counter = newCounter();
  counter->Add(42);
  EXPECT_EQ(42, counter->Count()) << "Adding 42 to 0";
}

TEST(DefaultCounterTest, Measure) {
  auto counter = newCounter();
  auto expected_id = *(Id("foo", kEmptyTags).WithTag(statistic::count));
  counter->Add(60);
  manual_clock.SetWall(60000);
  const Measurements& ms = counter->Measure();  // same a measure_for_poller(0);
  EXPECT_EQ(1, ms.size());

  for (auto& m : ms) {
    EXPECT_EQ(expected_id, *(m.id));
    EXPECT_EQ(60000, m.timestamp);
    EXPECT_DOUBLE_EQ(1.0, m.value);
  }

  // now we have two valid step_longs
  manual_clock.SetWall(110000);
  counter->Add(2);
  manual_clock.SetWall(120000);
  const Measurements& ms_60 = counter->Measure();
  EXPECT_EQ(1, ms_60.size());

  for (auto& m : ms_60) {
    EXPECT_EQ(expected_id, *(m.id));
    EXPECT_EQ(120000, m.timestamp);
    EXPECT_DOUBLE_EQ(2.0 / 60.0, m.value);
  }
}

TEST(DefaultCounter, Expiration) {
  ManualClock manual_clock;
  TestRegistry r{60000, &manual_clock};

  auto id = r.CreateId("test", kEmptyTags);
  r.counter(id)->Increment();

  r.SetWall(60000);
  EXPECT_EQ(r.measurements_for_name("test").size(), 1);

  auto t = atlas::meter::MAX_IDLE_TIME + 60000;
  r.SetWall(t);

  auto ms = r.measurements_for_name("test");
  EXPECT_EQ(ms.size(), 0);

  r.counter(id)->Increment();
  r.SetWall(t + 60000);
  ms = r.measurements_for_name("test");
  EXPECT_EQ(ms.size(), 1);
}
