#include "../interpreter/interpreter.h"
#include "../meter/percentile_dist_summary.h"
#include "test_registry.h"

#include <gtest/gtest.h>

using namespace atlas::meter;
using atlas::util::intern_str;

TEST(PercentileDistributionSummary, Percentile) {
  ManualClock manual_clock;
  TestRegistry r{&manual_clock};
  PercentileDistributionSummary d{&r, r.CreateId("foo", kEmptyTags)};

  for (auto i = 0; i < 100000; ++i) {
    d.Record(i);
  }

  for (auto i = 0; i <= 100; ++i) {
    auto expected = i * 1000.0;
    auto threshold = 0.15 * expected;
    EXPECT_NEAR(expected, d.Percentile(i), threshold);
  }
}

TEST(PercentileDistributionSummary, HasProperStatistic) {
  ManualClock manual_clock;
  TestRegistry r{&manual_clock};
  PercentileDistributionSummary t{&r, r.CreateId("foo", kEmptyTags)};
  t.Record(42);

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

TEST(PercentileDistributionSummary, CountTotal) {
  ManualClock manual_clock;
  TestRegistry r{&manual_clock};
  PercentileDistributionSummary d{&r, r.CreateId("foo", kEmptyTags)};

  for (auto i = 0; i < 100; ++i) {
    d.Record(i);
  }

  EXPECT_EQ(d.Count(), 100);
  EXPECT_EQ(d.TotalAmount(), 100 * 99 / 2);  // sum(1,n) = n * (n - 1) / 2
}
