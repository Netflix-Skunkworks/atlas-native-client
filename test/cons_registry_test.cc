#include "../meter/consolidation_registry.h"
#include "../util/config.h"
#include "test_utils.h"
#include <gtest/gtest.h>

using atlas::meter::ConsolidationRegistry;
using atlas::meter::Id;
using atlas::meter::Measurement;
using atlas::meter::Measurements;
using atlas::meter::Tags;
using atlas::util::kFastestFrequencyMillis;
using atlas::util::kMainFrequencyMillis;

TEST(ConsolidationRegistry, Empty) {
  ConsolidationRegistry registry{kFastestFrequencyMillis, kMainFrequencyMillis};
  auto ms = registry.measurements();
  ASSERT_TRUE(ms.empty());

  registry.update_from(Measurements{});
  ASSERT_TRUE(registry.measurements().empty());
}

bool operator==(const atlas::meter::Measurement& a,
                const atlas::meter::Measurement& b) {
  return a.timestamp == b.timestamp && a.id == b.id &&
         std::abs(a.value - b.value) < 1e-9;
}

TEST(ConsolidationRegistry, UpdateOne) {
  ConsolidationRegistry registry{kFastestFrequencyMillis, kMainFrequencyMillis};

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
  auto ms = registry.measurements();
  auto measure_cmp = [](const Measurement& a, const Measurement& b) {
    return *(a.id) < *(b.id);
  };

  // 60s for measurements
  auto counter_minute = Measurement{counter_id, 1L, 1 / 60.0};
  auto expected_measurements = Measurements{gauge, random, counter_minute};
  std::sort(expected_measurements.begin(), expected_measurements.end(),
            measure_cmp);
  std::sort(ms.begin(), ms.end(), measure_cmp);

  EXPECT_EQ(ms, expected_measurements);
  EXPECT_TRUE(registry.measurements().empty()) << "Resets after being measured";
}

static void update_timestamp(Measurements* measurements, int64_t ts) {
  for (auto& m : *measurements) {
    m.timestamp = ts;
  }
}

TEST(ConsolidationRegistry, Consolidate) {
  ConsolidationRegistry registry{kFastestFrequencyMillis, kMainFrequencyMillis};

  auto gauge_id = std::make_shared<Id>("gauge", Tags{{"statistic", "gauge"}});
  auto random_id = std::make_shared<Id>("random", Tags{});
  auto counter_id =
      std::make_shared<Id>("counter", Tags{{"statistic", "count"}});

  auto gauge = Measurement{gauge_id, 1L, 1.0};
  auto random = Measurement{random_id, 1L, 42.0};
  auto counter = Measurement{counter_id, 1L, 1 / 5.0};
  auto measurements = Measurements{gauge, random, counter};

  registry.update_from(measurements);

  update_timestamp(&measurements, 2L);
  registry.update_from(measurements);

  for (auto& m : measurements) {
    m.value += 1 / 5.0;
  }
  update_timestamp(&measurements, 3L);
  registry.update_from(measurements);
  auto ms = registry.measurements();
  auto measure_cmp = [](const Measurement& a, const Measurement& b) {
    return *(a.id) < *(b.id);
  };
  std::sort(ms.begin(), ms.end(), measure_cmp);

  ASSERT_EQ(ms.size(), 3);
  EXPECT_EQ(ms[0].id, counter.id);
  EXPECT_EQ(ms[0].value, 4.0 / 60);  // (0.2, 0.2, 0.4 => deltas = 1, 1, 2)
  EXPECT_EQ(ms[1].id, gauge.id);
  EXPECT_EQ(ms[1].value, 1.2);  // max
  EXPECT_EQ(ms[2].id, random.id);
  EXPECT_EQ(ms[2].value, 42.2);  // max
}

TEST(ConsolidationRegistry, IgnoreDupes) {
  ConsolidationRegistry registry{kFastestFrequencyMillis, kMainFrequencyMillis};

  auto counter_id =
      std::make_shared<Id>("counter", Tags{{"statistic", "count"}});

  auto counter = Measurement{counter_id, 1L, 1 / 5.0};
  auto measurements = Measurements{counter};

  registry.update_from(measurements);
  registry.update_from(measurements);
  registry.update_from(measurements);
  auto ms = registry.measurements();
  ASSERT_EQ(ms.size(), 1);
  EXPECT_EQ(ms[0].value, 1.0 / 60);
}
