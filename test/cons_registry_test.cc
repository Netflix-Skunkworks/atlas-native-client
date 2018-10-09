#include "../meter/consolidation_registry.h"
#include "../meter/manual_clock.h"
#include "../util/config.h"
#include "test_utils.h"
#include <gtest/gtest.h>

using atlas::meter::ConsolidationRegistry;
using atlas::meter::Id;
using atlas::meter::ManualClock;
using atlas::meter::Measurement;
using atlas::meter::Measurements;
using atlas::meter::Tags;
using atlas::util::kFastestFrequencyMillis;
using atlas::util::kMainFrequencyMillis;

static void update_timestamp(Measurements* measurements, int64_t ts) {
  for (auto& m : *measurements) {
    m.timestamp = ts;
  }
}

TEST(ConsolidationRegistry, Empty) {
  ManualClock clock;
  ConsolidationRegistry registry{kFastestFrequencyMillis, kMainFrequencyMillis,
                                 &clock};
  auto ms = registry.measurements(clock.WallTime());
  ASSERT_TRUE(ms.empty());

  registry.update_from(Measurements{});
  ASSERT_TRUE(registry.measurements(clock.WallTime()).empty());
}

bool operator==(const atlas::meter::Measurement& a,
                const atlas::meter::Measurement& b) {
  return a.timestamp == b.timestamp && a.id == b.id &&
         std::abs(a.value - b.value) < 1e-9;
}

TEST(ConsolidationRegistry, UpdateOne) {
  ManualClock clock;
  ConsolidationRegistry registry{kFastestFrequencyMillis, kMainFrequencyMillis,
                                 &clock};
  clock.SetWall(1000);

  auto gauge_id = std::make_shared<Id>("gauge", Tags{{"statistic", "gauge"}});
  auto random_id = std::make_shared<Id>("random", Tags{});
  auto counter_id =
      std::make_shared<Id>("counter", Tags{{"statistic", "count"}});

  auto gauge = Measurement{gauge_id, 1L, 1.0};
  auto random = Measurement{random_id, 1L, 42.0};
  // 5s for updates
  auto counter = Measurement{counter_id, 1L, 1 / 5.0};
  auto measurements = Measurements{gauge, random, counter};

  registry.update_from(measurements);
  clock.SetWall(60000);
  auto ms = registry.measurements(clock.WallTime());
  auto measure_cmp = [](const Measurement& a, const Measurement& b) {
    return *(a.id) < *(b.id);
  };

  // 60s for measurements
  auto counter_minute = Measurement{counter_id, clock.WallTime(), 1 / 60.0};
  auto expected_measurements = Measurements{gauge, random, counter_minute};
  update_timestamp(&expected_measurements, clock.WallTime());
  std::sort(expected_measurements.begin(), expected_measurements.end(),
            measure_cmp);
  std::sort(ms.begin(), ms.end(), measure_cmp);

  EXPECT_EQ(ms, expected_measurements);
}

TEST(ConsolidationRegistry, Consolidate) {
  ManualClock clock;
  ConsolidationRegistry registry{kFastestFrequencyMillis, kMainFrequencyMillis,
                                 &clock};

  auto gauge_id = std::make_shared<Id>("gauge", Tags{{"statistic", "gauge"}});
  auto random_id = std::make_shared<Id>("random", Tags{});
  auto counter_id =
      std::make_shared<Id>("counter", Tags{{"statistic", "count"}});

  clock.SetWall(1000);
  auto gauge = Measurement{gauge_id, clock.WallTime(), 1.0};
  auto random = Measurement{random_id, clock.WallTime(), 42.0};
  auto counter = Measurement{counter_id, clock.WallTime(), 1 / 5.0};
  auto measurements = Measurements{gauge, random, counter};

  registry.update_from(measurements);

  clock.SetWall(6000);
  update_timestamp(&measurements, clock.WallTime());
  registry.update_from(measurements);

  for (auto& m : measurements) {
    m.value += 1 / 5.0;
  }
  clock.SetWall(11000);
  update_timestamp(&measurements, clock.WallTime());
  registry.update_from(measurements);

  clock.SetWall(51000);
  update_timestamp(&measurements, clock.WallTime());
  registry.update_from(measurements);
  clock.SetWall(61000);
  auto ms = registry.measurements(clock.WallTime());
  auto measure_cmp = [](const Measurement& a, const Measurement& b) {
    return *(a.id) < *(b.id);
  };
  std::sort(ms.begin(), ms.end(), measure_cmp);

  ASSERT_EQ(ms.size(), 3);
  EXPECT_EQ(ms[0].id, counter.id);
  EXPECT_EQ(ms[0].value,
            6.0 / 60);  // (0.2, 0.2, 0.4, 0.4 => deltas = 1, 1, 2, 2)
  EXPECT_EQ(ms[1].id, gauge.id);
  EXPECT_EQ(ms[1].value, 1.2);  // max
  EXPECT_EQ(ms[2].id, random.id);
  EXPECT_EQ(ms[2].value, 42.2);  // max
}

TEST(ConsolidationRegistry, Expiration) {
  ManualClock clock;
  clock.SetWall(1000);
  ConsolidationRegistry registry{kFastestFrequencyMillis, kMainFrequencyMillis,
                                 &clock};

  auto gauge_id = std::make_shared<Id>("gauge", Tags{{"statistic", "gauge"}});
  auto counter_id =
      std::make_shared<Id>("counter", Tags{{"statistic", "count"}});
  auto gauge = Measurement{gauge_id, clock.WallTime(), 1.0};
  auto counter = Measurement{counter_id, clock.WallTime(), 1 / 5.0};
  auto measurements = Measurements{counter, gauge};

  registry.update_from(measurements);
  clock.SetWall(61000);
  auto ms = registry.measurements(clock.WallTime());
  EXPECT_EQ(registry.size(), 2);

  clock.SetWall(121000);
  ms = registry.measurements(clock.WallTime());
  EXPECT_TRUE(ms.empty());  // no data
  EXPECT_EQ(registry.size(), 2);

  clock.SetWall(181000);
  ms = registry.measurements(clock.WallTime());
  EXPECT_TRUE(ms.empty());  // no data
  EXPECT_EQ(registry.size(), 0);
}
