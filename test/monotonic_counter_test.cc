#include "../meter/monotonic_counter.h"
#include "test_registry.h"
#include <gtest/gtest.h>

using namespace atlas::meter;

TEST(MonotonicCounter, Init) {
  ManualClock manual_clock;
  TestRegistry r{&manual_clock};
  MonotonicCounter counter{&r, r.CreateId("ctr", kEmptyTags)};
  EXPECT_EQ(0, counter.Count()) << "A new counter should report a value of 0";
}

TEST(MonotonicCounter, Set) {
  ManualClock manual_clock;
  TestRegistry r{&manual_clock};
  MonotonicCounter counter{&r, r.CreateId("ctr", kEmptyTags)};
  counter.Set(100);

  EXPECT_EQ(100, counter.Count()) << "Should honor Set";

  counter.Set(200);
  EXPECT_EQ(200, counter.Count()) << "Should honor Set";
}

TEST(MonotonicCounter, NotEnoughData) {
  ManualClock manual_clock;
  TestRegistry r{&manual_clock};
  MonotonicCounter counter{&r, r.CreateId("ctr", kEmptyTags)};

  EXPECT_EQ(0, r.measurements_for_name("ctr").size())
      << "Not enough data for a rate";

  r.SetWall(61000);
  counter.Set(100);
  EXPECT_EQ(0, r.measurements_for_name("ctr").size())
      << "Not enough data for a rate";

  r.SetWall(121000);
  counter.Set(200);
  r.SetWall(181000);
  auto ms = r.measurements_for_name("ctr");
  EXPECT_EQ(1, ms.size()) << "Finally enough data";

  EXPECT_DOUBLE_EQ(100 / 60.0, ms.at(0).value) << "Computes the correct rate";
}

TEST(MonotonicCounter, Rate) {
  ManualClock manual_clock;
  TestRegistry r{&manual_clock};
  MonotonicCounter counter{&r, r.CreateId("ctr", kEmptyTags)};

  r.SetWall(1000);
  counter.Set(0);

  r.SetWall(61000);
  counter.Set(100);

  r.SetWall(62000);
  counter.Set(200);

  r.SetWall(121000);
  auto ms1 = r.measurements_for_name("ctr");
  EXPECT_DOUBLE_EQ(200 / 60.0, ms1.at(0).value) << "Computes the correct rate";
}

TEST(MonotonicCounter, Expiration) {
  ManualClock manual_clock;
  TestRegistry r{&manual_clock};
  MonotonicCounter counter{&r, r.CreateId("ctr", kEmptyTags)};

  r.SetWall(61000);
  counter.Set(100);

  EXPECT_FALSE(counter.HasExpired()) << "Recent update";

  r.SetWall(61000 + 14 * 60000);
  EXPECT_FALSE(counter.HasExpired()) << "Recent update";

  r.SetWall(61000 + 16 * 60000);
  EXPECT_TRUE(counter.HasExpired()) << "No recent update";

  EXPECT_EQ(0, r.measurements_for_name("ctr").size())
      << "Expired metrics are not reported";
}

TEST(MonotonicCounter, Overflow) {
  ManualClock manual_clock;
  TestRegistry r{&manual_clock};
  MonotonicCounter counter{&r, r.CreateId("ctr", kEmptyTags)};

  r.SetWall(1000);
  counter.Set(100);
  r.SetWall(61000);
  counter.Set(200);

  r.SetWall(121000);
  auto v = r.measurements_for_name("ctr").front().value;
  EXPECT_DOUBLE_EQ(100 / 60.0, v);  // baseline
  counter.Set(100);

  r.SetWall(181000);
  auto ov = r.measurements_for_name("ctr").front().value;
  EXPECT_DOUBLE_EQ(0.0, ov) << "Detects overflow in values";

  r.SetWall(241000);
  counter.Set(200);

  r.SetWall(301000);
  auto v2 = r.measurements_for_name("ctr").front().value;
  EXPECT_DOUBLE_EQ(100 / 60.0, v2) << "Recovers from overflow";
}
