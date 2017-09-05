#include "../meter/percentile_buckets.h"
#include <gtest/gtest.h>
#include <random>

namespace atlas {
namespace meter {
namespace percentile_buckets {
size_t leadingZeros(uint64_t v);
}  // namespace percentile_buckets
}  // namespace meter
}  // namespace atlas
using namespace atlas::meter;

TEST(PercentileBuckets, IndexOf) {
  EXPECT_EQ(0, percentile_buckets::IndexOf(-1));
  EXPECT_EQ(0, percentile_buckets::IndexOf(0));
  EXPECT_EQ(1, percentile_buckets::IndexOf(1));
  EXPECT_EQ(2, percentile_buckets::IndexOf(2));
  EXPECT_EQ(3, percentile_buckets::IndexOf(3));
  EXPECT_EQ(4, percentile_buckets::IndexOf(4));
  EXPECT_EQ(25, percentile_buckets::IndexOf(87));
  EXPECT_EQ(percentile_buckets::Length() - 1,
            percentile_buckets::IndexOf(std::numeric_limits<int64_t>::max()));
}

TEST(PercentileBuckets, LeadingZeros) {
  for (int i = 0; i < 64; i++) {
    uint64_t l = uint64_t{1} << i;
    EXPECT_EQ(63 - i, percentile_buckets::leadingZeros(l));
  }
}

TEST(PercentileBuckets, IndexOfSanityCheck) {
  std::mt19937_64 rng;
  rng.seed(std::random_device()());

  for (auto i = 0; i < 10000; ++i) {
    auto v = static_cast<int64_t>(rng());
    if (v < 0) {
      EXPECT_EQ(0, percentile_buckets::IndexOf(v));
    } else {
      auto b = percentile_buckets::Get(percentile_buckets::IndexOf(v));
      EXPECT_TRUE(v <= b) << v << " > " << b;
    }
  }
}

TEST(PercentileBuckets, BucketSanityCheck) {
  std::mt19937_64 rng;
  rng.seed(std::random_device()());

  for (auto i = 0; i < 10000; ++i) {
    auto v = static_cast<int64_t>(rng());
    if (v < 0) {
      EXPECT_EQ(1, percentile_buckets::Bucket(v));
    } else {
      auto b = percentile_buckets::Bucket(v);
      EXPECT_TRUE(v <= b) << v << " > " << b;
    }
  }
}

TEST(PercentileBuckets, Percentiles) {
  std::array<int64_t, percentile_buckets::Length()> counts;
  std::fill(counts.begin(), counts.end(), 0);

  for (int i = 0; i < 100000; ++i) {
    ++counts[percentile_buckets::IndexOf(i)];
  }

  std::vector<double> pcts{0.0,  25.0, 50.0, 75.0, 90.0,
                           95.0, 98.0, 99.0, 99.5, 100.0};
  std::vector<double> results;
  percentile_buckets::Percentiles(counts, pcts, &results);
  ASSERT_EQ(results.size(), pcts.size());

  std::vector<double> expected = {0.0,  25e3, 50e3, 75e3,   90e3,
                                  95e3, 98e3, 99e3, 99.5e3, 100e3};

  for (size_t i = 0; i < results.size(); ++i) {
    auto threshold = 0.1 * expected.at(i);
    EXPECT_NEAR(expected.at(i), results.at(i), threshold);
  }
}

TEST(PercentileBuckets, Percentile) {
  std::array<int64_t, percentile_buckets::Length()> counts;
  std::fill(counts.begin(), counts.end(), 0);

  for (int i = 0; i < 100000; ++i) {
    ++counts[percentile_buckets::IndexOf(i)];
  }

  std::vector<double> pcts{0.0,  25.0, 50.0, 75.0, 90.0,
                           95.0, 98.0, 99.0, 99.5, 100.0};
  std::vector<double> results;
  percentile_buckets::Percentiles(counts, pcts, &results);
  ASSERT_EQ(results.size(), pcts.size());

  for (size_t i = 0; i < pcts.size(); ++i) {
    auto expected = pcts[i] * 1e3;
    auto threshold = 0.1 * expected;
    EXPECT_NEAR(expected, percentile_buckets::Percentile(counts, pcts[i]),
                threshold);
  }
}