#include "../meter/percentile_timer.h"
#include "test_registry.h"
#include <gtest/gtest.h>

using namespace atlas::meter;
using atlas::util::intern_str;

TEST(PercentileTimer, Percentile) {
  ManualClock manual_clock;
  TestRegistry r{60000, &manual_clock};
  PercentileTimer t{&r, r.CreateId("foo", kEmptyTags)};

  for (auto i = 0; i < 100000; ++i) {
    t.Record(std::chrono::milliseconds{i});
  }

  for (auto i = 0; i <= 100; ++i) {
    auto expected = static_cast<double>(i);
    auto threshold = 0.15 * expected;
    EXPECT_NEAR(expected, t.Percentile(i), threshold);
  }
}

TEST(PercentileTimer, HasProperStatistic) {
  ManualClock manual_clock;
  TestRegistry r{60000, &manual_clock};
  PercentileTimer t{&r, r.CreateId("foo", kEmptyTags)};

  t.Record(std::chrono::milliseconds{42});

  auto ms = r.meters();
  auto percentileRef = intern_str("percentile");
  auto statisticRef = intern_str("statistic");
  for (const auto& m : ms) {
    auto tags = m->GetId()->GetTags();
    if (tags.has(percentileRef)) {
      EXPECT_EQ(tags.at(statisticRef), percentileRef);
    }
  }
}

TEST(PercentileTimer, CountTotal) {
  ManualClock manual_clock;
  TestRegistry r{60000, &manual_clock};
  PercentileTimer t{&r, r.CreateId("foo", kEmptyTags)};

  for (auto i = 0; i < 100; ++i) {
    t.Record(std::chrono::nanoseconds(i));
  }

  EXPECT_EQ(t.Count(), 100);
  EXPECT_EQ(t.TotalTime(), 100 * 99 / 2);  // sum(1,n) = n * (n - 1) / 2
}
