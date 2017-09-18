#include "../meter/manual_clock.h"
#include "../meter/statistic.h"
#include "../meter/subscription_distribution_summary.h"
#include "test_registry.h"
#include <gtest/gtest.h>

using namespace atlas::meter;

static ManualClock manual_clock;

static Pollers pollers;

static TestRegistry test_registry;
static auto id = test_registry.CreateId("foo", kEmptyTags);

static std::unique_ptr<SubscriptionDistributionSummary> newDistSummary() {
  pollers.clear();
  return std::make_unique<SubscriptionDistributionSummary>(id, manual_clock,
                                                           pollers);
}

TEST(SubDistSummary, Init) {
  auto t = newDistSummary();
  EXPECT_EQ(0, t->Count()) << "Initial count is 0";
  EXPECT_EQ(0, t->TotalAmount()) << "Initial totalAmount is 0";
  EXPECT_FALSE(t->HasExpired()) << "Not expired initially";

  // no subscriptions
  auto empty_ms = t->Measure();
  EXPECT_TRUE(empty_ms.empty());
}

TEST(SubDistSummary, Record) {
  auto t = newDistSummary();
  t->Record(42);
  EXPECT_EQ(1, t->Count()) << "Only one record called";
  EXPECT_EQ(42, t->TotalAmount());
}

TEST(SubDistSummary, RecordNegative) {
  auto t = newDistSummary();
  t->Record(-42);
  EXPECT_EQ(1, t->Count()) << "Negative values update the count";
  EXPECT_EQ(0, t->TotalAmount())
      << "Negative values ignored for the total amount";
}

TEST(SubDistSummary, RecordZero) {
  auto t = newDistSummary();
  t->Record(0);
  EXPECT_EQ(1, t->Count()) << "0 is counted";
  EXPECT_EQ(0, t->TotalAmount());
}

TEST(SubDistSummary, Measure) {
  auto t = newDistSummary();
  t->Record(42);

  const auto& ms = t->Measure();
  EXPECT_EQ(0, ms.size());

  pollers.push_back(60000);
  t->UpdatePollers();

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
    } else if (*(m.id) ==
               *(Id("foo", kEmptyTags)
                     .WithTag(statistic::max)
                     ->WithTag(Tag::of("atlas.dstype", "gauge")))) {
      EXPECT_DOUBLE_EQ(44, m.value);
    } else {
      FAIL() << "Unknown id from measurement: " << *m.id;
    }
  }
}
