#include "../meter/subscription_timer.h"
#include "../meter/manual_clock.h"
#include "../meter/statistic.h"
#include "test_registry.h"
#include <gtest/gtest.h>

using namespace atlas::meter;

static ManualClock manual_clock;

static Pollers pollers;

static TestRegistry test_registry;
static auto id = test_registry.CreateId("foo", kEmptyTags);

static std::unique_ptr<SubscriptionTimer> newTimer() {
  pollers.clear();
  return std::make_unique<SubscriptionTimer>(id, manual_clock, pollers);
}

TEST(SubTimer, Init) {
  auto t = newTimer();
  EXPECT_EQ(0, t->Count()) << "Initial count is 0";
  EXPECT_EQ(0, t->TotalTime()) << "Initial totalTime is 0";
  EXPECT_FALSE(t->HasExpired()) << "Not expired initially";

  // no subscriptions
  auto empty_ms = t->Measure();
  EXPECT_TRUE(empty_ms.empty());
}

TEST(SubTimer, Record) {
  auto t = newTimer();
  t->Record(std::chrono::milliseconds(42));
  EXPECT_EQ(1, t->Count()) << "Only one record called";
  EXPECT_EQ(42 * 1000000, t->TotalTime()) << "Unit conversions work";
}

TEST(SubTimer, RecordNegative) {
  auto t = newTimer();
  t->Record(std::chrono::milliseconds(-42));
  EXPECT_EQ(1, t->Count()) << "Negative values update the count";
  EXPECT_EQ(0, t->TotalTime()) << "Negative values ignored for the total time";
}

TEST(SubTimer, RecordZero) {
  auto t = newTimer();
  t->Record(std::chrono::milliseconds(0));
  EXPECT_EQ(1, t->Count()) << "0 duration is counted";
  EXPECT_EQ(0, t->TotalTime());
}

TEST(SubTimer, Measure) {
  auto t = newTimer();
  t->Record(std::chrono::milliseconds(42));

  const auto& ms = t->Measure();
  EXPECT_EQ(0, ms.size());

  pollers.push_back(60000);
  t->UpdatePollers();

  manual_clock.SetWall(60000);
  t->Record(std::chrono::milliseconds(40));
  t->Record(std::chrono::milliseconds(42));
  t->Record(std::chrono::milliseconds(44));

  manual_clock.SetWall(120000);
  const auto& one = t->Measure();

  EXPECT_EQ(4, one.size());
  for (auto& m : one) {
    EXPECT_EQ(120000, m.timestamp);
    if (*(m.id) == *(Id("foo", kEmptyTags).WithTag(statistic::count))) {
      EXPECT_DOUBLE_EQ(3 / 60.0, m.value);
    } else if (*(m.id) ==
               *(Id("foo", kEmptyTags).WithTag(statistic::totalTime))) {
      EXPECT_DOUBLE_EQ(126 / 60000.0, m.value);
    } else if (*(m.id) ==
               *(Id("foo", kEmptyTags)
                     .WithTag(statistic::max)
                     ->WithTag(Tag::of("atlas.dstype", "gauge")))) {
      EXPECT_DOUBLE_EQ(44 / 1000.0, m.value);
    } else if (*(m.id) ==
               *(Id("foo", kEmptyTags).WithTag(statistic::totalOfSquares))) {
      auto sum_sq = 40.0 * 1e6 * 40.0 * 1e6 + 42.0 * 1e6 * 42.0 * 1e6 +
                    44.0 * 1e6 * 44.0 * 1e6;
      auto factor = 1e9 * 1e9;
      EXPECT_DOUBLE_EQ(sum_sq / factor / 60.0, m.value);
    } else {
      FAIL() << "Unknown id from measurement: " << *m.id;
    }
  }
}
