#include "../meter/manual_clock.h"
#include "../meter/stepnumber.h"
#include <gtest/gtest.h>

using namespace ::atlas::meter;

TEST(StepLong, Init) {
  ManualClock manual_clock;
  StepLong s(0, 10, &manual_clock);
  EXPECT_EQ(0, s.Current());
  EXPECT_EQ(0, s.Poll());
}

TEST(StepLong, Increment) {
  ManualClock manual_clock;
  StepLong s(0, 10, &manual_clock);
  s.Add(1);
  EXPECT_EQ(1, s.Current());
  EXPECT_EQ(0, s.Poll());
}

TEST(StepLong, IncrementAndCrossStep) {
  ManualClock manual_clock;
  StepLong s(0, 10, &manual_clock);
  s.Add(1);
  manual_clock.SetWall(10);
  EXPECT_EQ(0, s.Current());
  EXPECT_EQ(1, s.Poll());
}

TEST(StepLong, MissedRead) {
  ManualClock manual_clock;
  StepLong s(0, 10, &manual_clock);
  s.Add(1);
  manual_clock.SetWall(20);
  EXPECT_EQ(0, s.Current());
  EXPECT_EQ(0, s.Poll());
}