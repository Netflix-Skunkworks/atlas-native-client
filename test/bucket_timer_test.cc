#include "../meter/bucket_timer.h"
#include "../util/logger.h"
#include "test_registry.h"
#include <gtest/gtest.h>

using atlas::meter::BucketTimer;
using atlas::meter::Measurement;
using atlas::meter::Tag;
using atlas::meter::bucket_functions::Age;
using atlas::meter::kEmptyTags;
using atlas::util::Logger;
using std::chrono::milliseconds;

TEST(BucketTimer, Init) {
  TestRegistry r;

  auto id = r.CreateId("test", kEmptyTags);
  BucketTimer t(&r, id, Age(milliseconds{100}));
  Logger()->info("Created: {}", t);

  auto ms = r.AllMeasurements();
  EXPECT_EQ(ms.size(), 0);
}

static constexpr auto kMillisToSecs = 1 / 1000.0;

TEST(BucketTimer, Record) {
  TestRegistry r;
  r.SetWall(1000);

  auto id = r.CreateId("test", kEmptyTags);
  BucketTimer ds(&r, id, Age(milliseconds{100}));
  ds.Record(milliseconds(30));
  ds.Record(milliseconds(28));

  r.SetWall(61000);
  auto ms = r.AllMeasurements();
  EXPECT_EQ(ms.size(), 4);

  auto base_id = id->WithTag(Tag::of("bucket", "050ms"));
  for (const auto& m : ms) {
    EXPECT_EQ(60000, m.timestamp);

    if (*m.id == *base_id->WithTag(atlas::meter::statistic::count)) {
      EXPECT_DOUBLE_EQ(2 / 60.0, m.value);
    } else if (*m.id == *base_id->WithTag(atlas::meter::statistic::totalTime)) {
      auto total = 30 * kMillisToSecs + 28 * kMillisToSecs;
      EXPECT_DOUBLE_EQ(total / 60.0, m.value);
    } else if (*m.id ==
               *base_id->WithTag(atlas::meter::statistic::totalOfSquares)) {
      auto totalSq = 30.0 * kMillisToSecs * 30.0 * kMillisToSecs +
                     28.0 * kMillisToSecs * 28.0 * kMillisToSecs;
      EXPECT_DOUBLE_EQ(totalSq / 60.0, m.value);
    } else if (*m.id ==
               *base_id->WithTag(atlas::meter::statistic::max)
                    ->WithTag(Tag::of("atlas.dstype", "gauge"))) {
      EXPECT_DOUBLE_EQ(30 * kMillisToSecs, m.value);
    } else {
      FAIL() << "Unknown id: " << *m.id;
    }
  }
}
