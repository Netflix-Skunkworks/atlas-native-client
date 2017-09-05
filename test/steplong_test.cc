#include "../meter/manual_clock.h"
#include "../meter/stepnumber.h"
#include <gtest/gtest.h>

using namespace ::atlas::meter;
static ManualClock manual_clock;

TEST(StepLong, Init) {
  StepLong s(0, 10, manual_clock);
  EXPECT_EQ(0, s.Current());
  EXPECT_EQ(0, s.Poll());
}

TEST(StepLong, Increment) {
  StepLong s(0, 10, manual_clock);
  s.Add(1);
  EXPECT_EQ(1, s.Current());
  EXPECT_EQ(0, s.Poll());
}

TEST(StepLong, IncrementAndCrossStep) {
  StepLong s(0, 10, manual_clock);
  s.Add(1);
  manual_clock.SetWall(10);
  EXPECT_EQ(0, s.Current());
  EXPECT_EQ(1, s.Poll());
  manual_clock.SetWall(0);
}

TEST(StepLong, MissedRead) {
  StepLong s(0, 10, manual_clock);
  s.Add(1);
  manual_clock.SetWall(20);
  EXPECT_EQ(0, s.Current());
  EXPECT_EQ(0, s.Poll());
  manual_clock.SetWall(0);
}