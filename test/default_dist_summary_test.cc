#include "../meter/manual_clock.h"
#include "../meter/statistic.h"
#include "../meter/default_distribution_summary.h"
#include "test_registry.h"
#include <gtest/gtest.h>

using namespace atlas::meter;
using atlas::util::kMainFrequencyMillis;

static ManualClock manual_clock;
static TestRegistry test_registry{&manual_clock};
static auto id = test_registry.CreateId("foo", kEmptyTags);

static std::unique_ptr<DefaultDistributionSummary> newDistSummary() {
  return std::make_unique<DefaultDistributionSummary>(id, manual_clock,
                                                      kMainFrequencyMillis);
}

TEST(DefaultDistSummary, Init) {
  auto t = newDistSummary();
  EXPECT_EQ(0, t->Count()) << "Initial count is 0";
  EXPECT_EQ(0, t->TotalAmount()) << "Initial totalAmount is 0";
  EXPECT_FALSE(t->HasExpired()) << "Not expired initially";
}

TEST(DefaultDistSummary, Record) {
  auto t = newDistSummary();
  t->Record(42);
  EXPECT_EQ(1, t->Count()) << "Only one record called";
  EXPECT_EQ(42, t->TotalAmount());
}

TEST(DefaultDistSummary, RecordNegative) {
  auto t = newDistSummary();
  t->Record(-42);
  EXPECT_EQ(0, t->Count()) << "Negative values do not update the count";
  EXPECT_EQ(0, t->TotalAmount())
      << "Negative values ignored for the total amount";
}

TEST(DefaultDistSummary, RecordZero) {
  auto t = newDistSummary();
  t->Record(0);
  EXPECT_EQ(1, t->Count()) << "0 is counted";
  EXPECT_EQ(0, t->TotalAmount());
}

TEST(DefaultDistSummary, Measure) {
  auto t = newDistSummary();
  t->Record(42);

  manual_clock.SetWall(60000);
  t->Record(40);
  t->Record(42);
  t->Record(44);

  manual_clock.SetWall(120000);
  const auto& one = t->Measure();

  EXPECT_EQ(4, one.size());
  for (auto& m : one) {
    EXPECT_EQ(120000, m.timestamp);
    if (*(m.id) == *(Id("foo", kEmptyTags).WithTag(statistic::count))) {
      EXPECT_DOUBLE_EQ(3 / 60.0, m.value);
    } else if (*(m.id) ==
               *(Id("foo", kEmptyTags).WithTag(statistic::totalAmount))) {
      EXPECT_DOUBLE_EQ(126 / 60.0, m.value);
    } else if (*(m.id) ==
               *(Id("foo", kEmptyTags).WithTag(statistic::totalOfSquares))) {
      auto totalSq = 40.0 * 40.0 + 42.0 * 42.0 + 44.0 * 44.0;
      EXPECT_DOUBLE_EQ(totalSq / 60.0, m.value);
    } else if (*(m.id) == *(Id("foo", kEmptyTags)
                                .WithTag(statistic::max)
                                ->WithTag(Tag::of("atlas.dstype", "gauge")))) {
      EXPECT_DOUBLE_EQ(44, m.value);
    } else {
      FAIL() << "Unknown id from measurement: " << *m.id;
    }
  }
}
