
#include "../meter/interval_counter.h"
#include "../meter/statistic.h"
#include "test_registry.h"
#include <gtest/gtest.h>

using atlas::meter::Id;
using atlas::meter::IntervalCounter;
using atlas::meter::Measurements;
using atlas::meter::kEmptyTags;
using atlas::util::intern_str;

static std::unique_ptr<TestRegistry> registry() {
  return std::make_unique<TestRegistry>();
}

TEST(IntervalCounter, Init) {
  auto r = registry();
  r->SetWall(42 * 1000);
  auto id = r->CreateId("test", kEmptyTags);
  IntervalCounter c{r.get(), id};

  EXPECT_EQ(c.Count(), 0);
  EXPECT_DOUBLE_EQ(c.SecondsSinceLastUpdate(), 42.0);
}

TEST(IntervalCounter, Interval) {
  auto r = registry();
  r->SetWall(0);
  auto id = r->CreateId("test", kEmptyTags);
  IntervalCounter c{r.get(), id};
  EXPECT_DOUBLE_EQ(c.SecondsSinceLastUpdate(), 0.0);
  r->SetWall(1000L);
  EXPECT_DOUBLE_EQ(c.SecondsSinceLastUpdate(), 1.0);
  c.Increment();
  EXPECT_DOUBLE_EQ(c.SecondsSinceLastUpdate(), 0.0);
  r->SetWall(3000L);
  EXPECT_DOUBLE_EQ(c.SecondsSinceLastUpdate(), 2.0);
  c.Add(42);
  EXPECT_DOUBLE_EQ(c.SecondsSinceLastUpdate(), 0.0);
}

TEST(IntervalCounter, Increment) {
  auto r = registry();
  auto id = r->CreateId("test", kEmptyTags);
  IntervalCounter c{r.get(), id};

  EXPECT_EQ(c.Count(), 0);
  c.Increment();
  EXPECT_EQ(c.Count(), 1);
  c.Add(41);
  EXPECT_EQ(c.Count(), 42);
}

void assert_interval_counter(const Measurements& ms, int64_t timestamp,
                             double count, double interval) {
  auto statisticRef = intern_str("statistic");
  EXPECT_EQ(ms.size(), 2);
  for (const auto& m : ms) {
    EXPECT_EQ(m.timestamp, timestamp) << "Wrong timestamp for " << m;
    const auto& stat = m.id->GetTags().at(statisticRef);
    if (atlas::meter::statistic::count.value == stat) {
      EXPECT_DOUBLE_EQ(m.value, count);
    } else {
      EXPECT_EQ(stat, atlas::meter::statistic::duration.value);
      EXPECT_DOUBLE_EQ(m.value, interval);
    }
  }
}

TEST(IntervalCounter, Measure) {
  auto r = registry();
  auto id = r->CreateId("test", kEmptyTags);
  IntervalCounter c{r.get(), id};

  c.Increment();
  r->SetWall(60000);
  assert_interval_counter(r->AllMeasurements(), 60000, 1 / 60.0, 60.0);

  r->SetWall(120000);
  assert_interval_counter(r->AllMeasurements(), 120000, 0 / 60.0, 120.0);

  c.Increment();
  r->SetWall(180000);
  assert_interval_counter(r->AllMeasurements(), 180000, 1 / 60.0, 60.0);
}

TEST(IntervalCounter, ReusesInstance) {
  auto r = registry();

  auto id = r->CreateId("test", kEmptyTags);

  IntervalCounter c1{r.get(), id};
  IntervalCounter c2{r.get(), id};

  c1.Increment();
  c2.Increment();
  r->SetWall(60000);

  assert_interval_counter(r->AllMeasurements(), 60000, 2 / 60.0, 60.0);
}

TEST(IntervalCounter, Expiration) {
  // make sure we always report the interval since last update
  auto r = registry();

  auto id = r->CreateId("test", kEmptyTags);
  IntervalCounter c{r.get(), id};
  c.Increment();

  r->SetWall(60000);
  EXPECT_EQ(r->AllMeasurements().size(), 2);

  auto t = atlas::meter::MAX_IDLE_TIME + 60000;
  r->SetWall(t);

  auto ms = r->AllMeasurements();
  EXPECT_EQ(ms.size(), 1);
  EXPECT_DOUBLE_EQ(ms.at(0).value, t / 1000.0);
}
