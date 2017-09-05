#include "../meter/bucket_functions.h"
#include <gtest/gtest.h>

using namespace atlas::meter;
using namespace std::chrono;

static int64_t ms(int millis) {
  return nanoseconds{milliseconds{millis}}.count();
}
static int64_t s(int secs) { return nanoseconds{seconds{secs}}.count(); }

TEST(BucketFunctions, Age) {
  auto f = bucket_functions::Age(seconds{60});

  EXPECT_EQ(f(s(-1)), "future");
  EXPECT_EQ(f(s(0)), "07s");
  EXPECT_EQ(f(s(1)), "07s");
  EXPECT_EQ(f(s(6)), "07s");
  EXPECT_EQ(f(s(7)), "07s");
  EXPECT_EQ(f(s(8)), "15s");
  EXPECT_EQ(f(s(10)), "15s");
  EXPECT_EQ(f(s(20)), "30s");
  EXPECT_EQ(f(s(30)), "30s");
  EXPECT_EQ(f(s(31)), "60s");
  EXPECT_EQ(f(s(42)), "60s");
  EXPECT_EQ(f(s(60)), "60s");
  EXPECT_EQ(f(s(61)), "old");
}

TEST(BucketFunctions, AgeBiasOld) {
  auto f = bucket_functions::AgeBiasOld(seconds{60});

  EXPECT_EQ(f(s(-1)), "future");
  EXPECT_EQ(f(s(0)), "30s");
  EXPECT_EQ(f(s(1)), "30s");
  EXPECT_EQ(f(s(6)), "30s");
  EXPECT_EQ(f(s(7)), "30s");
  EXPECT_EQ(f(s(10)), "30s");
  EXPECT_EQ(f(s(20)), "30s");
  EXPECT_EQ(f(s(30)), "30s");
  EXPECT_EQ(f(s(42)), "45s");
  EXPECT_EQ(f(s(48)), "52s");
  EXPECT_EQ(f(s(59)), "60s");
  EXPECT_EQ(f(s(60)), "60s");
  EXPECT_EQ(f(s(61)), "old");
}

TEST(BucketFunctions, Latency100ms) {
  auto f = bucket_functions::Latency(milliseconds{100});
  EXPECT_EQ(f(ms(-1)), "negative_latency");
  EXPECT_EQ(f(ms(0)), "012ms");
  EXPECT_EQ(f(ms(1)), "012ms");
  EXPECT_EQ(f(ms(13)), "025ms");
  EXPECT_EQ(f(ms(25)), "025ms");
  EXPECT_EQ(f(ms(99)), "100ms");
  EXPECT_EQ(f(ms(100)), "100ms");
  EXPECT_EQ(f(ms(101)), "slow");
}

TEST(BucketFunctions, Latency100msBiasSlow) {
  auto f = bucket_functions::LatencyBiasSlow(milliseconds{100});
  EXPECT_EQ(f(ms(-1)), "negative_latency");
  EXPECT_EQ(f(ms(0)), "050ms");
  EXPECT_EQ(f(ms(1)), "050ms");
  EXPECT_EQ(f(ms(13)), "050ms");
  EXPECT_EQ(f(ms(25)), "050ms");
  EXPECT_EQ(f(ms(74)), "075ms");
  EXPECT_EQ(f(ms(75)), "075ms");
  EXPECT_EQ(f(ms(76)), "087ms");
  EXPECT_EQ(f(ms(99)), "100ms");
  EXPECT_EQ(f(ms(100)), "100ms");
  EXPECT_EQ(f(ms(101)), "slow");
}

TEST(BucketFunctions, Latency3s) {
  auto f = bucket_functions::Latency(seconds{3});
  EXPECT_EQ(f(ms(-1)), "negative_latency");
  EXPECT_EQ(f(ms(0)), "0375ms");
  EXPECT_EQ(f(ms(25)), "0375ms");
  EXPECT_EQ(f(ms(740)), "0750ms");
  EXPECT_EQ(f(ms(1000)), "1500ms");
  EXPECT_EQ(f(ms(1567)), "3000ms");
  EXPECT_EQ(f(ms(3101)), "slow");
}

TEST(BucketFunctions, Bytes1k) {
  auto f = bucket_functions::Bytes(1024);
  EXPECT_EQ(f(-1), "negative");
  EXPECT_EQ(f(212), "0256_B");
  EXPECT_EQ(f(512), "0512_B");
  EXPECT_EQ(f(761), "1024_B");
  EXPECT_EQ(f(2000), "large");
}

TEST(BucketFunctions, Bytes20k) {
  auto f = bucket_functions::Bytes(20000);
  EXPECT_EQ(f(-1), "negative");
  EXPECT_EQ(f(761), "02_KiB");
  EXPECT_EQ(f(4567), "04_KiB");
  EXPECT_EQ(f(15761), "19_KiB");
  EXPECT_EQ(f(20001), "large");
}

TEST(BucketFunctions, BytesMax) {
  auto maxValue = std::numeric_limits<int64_t>::max();
  auto f = bucket_functions::Bytes(maxValue);
  EXPECT_EQ(f(-1), "negative");
  EXPECT_EQ(f(761), "1023_PiB");
  EXPECT_EQ(f(maxValue / 4), "2047_PiB");
  EXPECT_EQ(f(maxValue / 2), "4095_PiB");
  EXPECT_EQ(f(maxValue), "8191_PiB");
}

TEST(BucketFunctions, Decimal20k) {
  auto f = bucket_functions::Decimal(20000);
  EXPECT_EQ(f(-1), "negative");
  EXPECT_EQ(f(761), "02_k");
  EXPECT_EQ(f(4567), "05_k");
  EXPECT_EQ(f(15761), "20_k");
  EXPECT_EQ(f(20001), "large");
}

TEST(BucketFunctions, DecimalMax) {
  auto maxValue = std::numeric_limits<int64_t>::max();
  auto f = bucket_functions::Decimal(maxValue);
  EXPECT_EQ(f(-1), "negative");
  EXPECT_EQ(f(761), "1_E");
  EXPECT_EQ(f(maxValue / 4), "2_E");
  EXPECT_EQ(f(maxValue / 2), "4_E");
  EXPECT_EQ(f(maxValue), "9_E");
}
