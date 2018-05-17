#include "../meter/default_timer.h"
#include "../meter/manual_clock.h"
#include "../meter/statistic.h"
#include "test_registry.h"
#include <gtest/gtest.h>

using namespace atlas::meter;
using atlas::util::kMainFrequencyMillis;

static ManualClock manual_clock;

static std::unique_ptr<DefaultTimer> newTimer() {
  static auto id = std::make_shared<Id>("foo", kEmptyTags);
  return std::make_unique<DefaultTimer>(id, manual_clock, kMainFrequencyMillis);
}

TEST(DefaultTimer, Init) {
  auto t = newTimer();
  EXPECT_EQ(0, t->Count()) << "Initial count is 0";
  EXPECT_EQ(0, t->TotalTime()) << "Initial totalTime is 0";
  EXPECT_FALSE(t->HasExpired()) << "Not expired initially";
}

TEST(DefaultTimer, Record) {
  auto t = newTimer();
  t->Record(std::chrono::milliseconds(42));
  EXPECT_EQ(1, t->Count()) << "Only one record called";
  EXPECT_EQ(42 * 1000000, t->TotalTime()) << "Unit conversions work";
}

TEST(DefaultTimer, RecordNegative) {
  auto t = newTimer();
  t->Record(std::chrono::milliseconds(-42));
  EXPECT_EQ(0, t->Count()) << "Negative values do not update the count";
  EXPECT_EQ(0, t->TotalTime()) << "Negative values ignored for the total time";
}

TEST(DefaultTimer, RecordZero) {
  auto t = newTimer();
  t->Record(std::chrono::milliseconds(0));
  EXPECT_EQ(1, t->Count()) << "0 duration is counted";
  EXPECT_EQ(0, t->TotalTime());
}

TEST(DefaultTimer, Measure) {
  auto t = newTimer();
  t->Record(std::chrono::milliseconds(42));

  const auto& ms = t->Measure();
  EXPECT_EQ(4, ms.size());

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
    } else if (*(m.id) == *(Id("foo", kEmptyTags)
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

TEST(DefaultTimer, Expiration) {
  ManualClock manual_clock;
  TestRegistry r{&manual_clock};

  auto id = r.CreateId("test", kEmptyTags);
  r.timer(id)->Record(1);

  r.SetWall(60000);
  EXPECT_EQ(r.measurements_for_name("test").size(), 4);

  auto t = atlas::meter::MAX_IDLE_TIME + 60000;
  r.SetWall(t);

  auto ms = r.measurements_for_name("test");
  EXPECT_EQ(ms.size(), 0);

  r.timer(id)->Record(2);
  r.SetWall(t + 60000);
  ms = r.measurements_for_name("test");
  EXPECT_EQ(ms.size(), 4);
}
