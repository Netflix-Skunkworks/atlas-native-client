#include "../interpreter/interpreter.h"
#include "../meter/percentile_dist_summary.h"
#include "../meter/subscription_registry.h"

#include <gtest/gtest.h>

using namespace atlas::meter;
using atlas::interpreter::Interpreter;
using atlas::interpreter::ClientVocabulary;
using atlas::util::intern_str;

TEST(PercentileDistributionSummary, Percentile) {
  SubscriptionRegistry atlas_registry{
      std::make_unique<Interpreter>(std::make_unique<ClientVocabulary>())};
  PercentileDistributionSummary d{&atlas_registry,
                                  atlas_registry.CreateId("foo", kEmptyTags)};

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
  SubscriptionRegistry atlas_registry{
      std::make_unique<Interpreter>(std::make_unique<ClientVocabulary>())};
  PercentileDistributionSummary t{&atlas_registry,
                                  atlas_registry.CreateId("foo", kEmptyTags)};

  t.Record(42);

  auto ms = atlas_registry.meters();
  auto percentileRef = intern_str("percentile");
  auto statisticRef = intern_str("statistic");
  for (auto i = ms.begin(); i != ms.end(); ++i) {
    auto tags = (*i)->GetId()->GetTags();
    if (tags.has(percentileRef)) {
      EXPECT_EQ(tags.at(statisticRef), percentileRef);
    }
  }
}
