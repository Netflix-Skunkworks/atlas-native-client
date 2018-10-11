#include "../meter/bucket_distribution_summary.h"
#include "../meter/statistic.h"
#include "../util/logger.h"
#include "test_registry.h"
#include <gtest/gtest.h>

using atlas::meter::BucketDistributionSummary;
using atlas::meter::kEmptyTags;
using atlas::meter::ManualClock;
using atlas::meter::Measurement;
using atlas::meter::Tag;
using atlas::meter::bucket_functions::Age;
using atlas::util::Logger;
using std::chrono::seconds;

TEST(BucketDistributionSummary, Init) {
  ManualClock m;
  TestRegistry r{60000, &m};

  auto id = r.CreateId("test", kEmptyTags);
  BucketDistributionSummary ds(&r, id, Age(seconds{60}));
  auto ms = r.measurements_for_name("test");
  EXPECT_EQ(ms.size(), 0);
}

// seconds to nanos
static constexpr int64_t kSecsToNanos = 1000l * 1000l * 1000l;

TEST(BucketDistributionSummary, Record) {
  ManualClock manual_clock;
  TestRegistry r{60000, &manual_clock};
  r.SetWall(1000);

  auto id = r.CreateId("test", kEmptyTags);
  BucketDistributionSummary ds(&r, id, Age(seconds{60}));
  ds.Record(30 * kSecsToNanos);
  ds.Record(22 * kSecsToNanos);

  r.SetWall(61000);
  auto ms = r.measurements_for_name("test");
  EXPECT_EQ(ms.size(), 4);

  auto base_id = id->WithTag(Tag::of("bucket", "30s"));
  for (const auto& m : ms) {
    EXPECT_EQ(60000, m.timestamp);

    if (*m.id == *base_id->WithTag(atlas::meter::statistic::count)) {
      EXPECT_DOUBLE_EQ(2 / 60.0, m.value);
    } else if (*m.id ==
               *base_id->WithTag(atlas::meter::statistic::totalAmount)) {
      auto total = 30 * kSecsToNanos + 22 * kSecsToNanos;
      EXPECT_DOUBLE_EQ(total / 60.0, m.value);
    } else if (*m.id ==
               *base_id->WithTag(atlas::meter::statistic::totalOfSquares)) {
      auto totalSq = 30.0 * kSecsToNanos * 30.0 * kSecsToNanos +
                     22.0 * kSecsToNanos * 22.0 * kSecsToNanos;
      EXPECT_DOUBLE_EQ(totalSq / 60.0, m.value);
    } else if (*m.id == *base_id->WithTag(atlas::meter::statistic::max)
                             ->WithTag(Tag::of("atlas.dstype", "gauge"))) {
      EXPECT_DOUBLE_EQ(30 * kSecsToNanos, m.value);
    } else {
      FAIL() << "Unknown id: " << *m.id;
    }
  }
}
