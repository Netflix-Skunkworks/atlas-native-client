#include "../meter/bucket_counter.h"
#include "../meter/statistic.h"
#include "../util/logger.h"
#include "test_registry.h"
#include <gtest/gtest.h>

using atlas::meter::BucketCounter;
using atlas::meter::kEmptyTags;
using atlas::meter::ManualClock;
using atlas::meter::Measurement;
using atlas::meter::Measurements;
using atlas::meter::Registry;
using atlas::meter::Tag;
using atlas::meter::bucket_functions::Age;
using atlas::util::Logger;
using std::chrono::microseconds;

TEST(BucketCounter, Init) {
  ManualClock m;
  TestRegistry r{60000, &m};
  auto id = r.CreateId("test", kEmptyTags);
  BucketCounter b(&r, id, Age(microseconds{100}));
  auto ms = r.measurements_for_name("test");
  EXPECT_EQ(ms.size(), 0);
}

// micros to nanos
static constexpr int64_t kMicrosToNanos = 1000l;
TEST(BucketCounter, Record) {
  ManualClock manual_clock;
  TestRegistry r{60000, &manual_clock};
  r.SetWall(1000);

  auto id = r.CreateId("test", kEmptyTags);
  BucketCounter b(&r, id, Age(microseconds{100}));
  b.Record(30 * kMicrosToNanos);

  r.SetWall(61000);
  auto ms = r.measurements_for_name("test");
  EXPECT_EQ(ms.size(), 1);

  auto m = ms.front();

  auto expected_id = id->WithTag(atlas::meter::statistic::count)
                         ->WithTag(Tag::of("bucket", "050us"));
  auto expected_value = 1 / 60.0;
  EXPECT_EQ(*expected_id, *m.id);
  EXPECT_DOUBLE_EQ(expected_value, m.value);
}
