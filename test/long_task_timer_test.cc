#include "../meter/long_task_timer.h"
#include "../meter/manual_clock.h"
#include "../meter/statistic.h"
#include "../meter/subscription_long_task_timer.h"
#include "test_registry.h"
#include <gtest/gtest.h>

using namespace atlas::meter;

static ManualClock manual_clock;
static TestRegistry test_registry;
static auto id = test_registry.CreateId("foo", kEmptyTags);

static std::unique_ptr<SubscriptionLongTaskTimer> newTimer() {
  return std::make_unique<SubscriptionLongTaskTimer>(id, manual_clock);
}

TEST(LongTaskTimerTest, Init) {
  auto t = newTimer();
  EXPECT_EQ(0, t->Duration()) << "Initial count is 0";
  EXPECT_EQ(0, t->ActiveTasks()) << "Initial totalTime is 0";
  EXPECT_FALSE(t->HasExpired()) << "Not expired initially";
}

TEST(LongTaskTimerTest, Start) {
  auto t = newTimer();

  auto task1 = t->Start();
  auto task2 = t->Start();

  EXPECT_NE(task1, task2) << "Different tasks get different ids";
  EXPECT_EQ(2, t->ActiveTasks());
  EXPECT_EQ(0, t->Duration()) << "No tasks have finished";
}

TEST(LongTaskTimerTest, Stop) {
  auto t = newTimer();

  auto task1 = t->Start();
  auto task2 = t->Start();

  EXPECT_EQ(2, t->ActiveTasks());

  manual_clock.SetMonotonic(5);

  EXPECT_EQ(10, t->Duration()) << "Sum of all durations = 10";

  auto elapsed = t->Stop(task1);

  EXPECT_EQ(-1, t->Stop(task1));
  EXPECT_EQ(5, elapsed);
  EXPECT_EQ(5, t->Duration(task2));
  EXPECT_EQ(-1, t->Duration(task1));
  EXPECT_EQ(5, t->Duration());
  EXPECT_EQ(1, t->ActiveTasks());
}

static void assertLongTaskTimer(const SubscriptionLongTaskTimer& t,
                                int64_t timestamp, int activeTasks,
                                double duration) {
  bool tested_something = false;
  for (auto m : t.Measure()) {
    tested_something = true;
    EXPECT_EQ(timestamp, m.timestamp);
    if (*(m.id) == *(t.GetId()->WithTag(statistic::activeTasks))) {
      EXPECT_DOUBLE_EQ(activeTasks, m.value) << "Active Tasks";
    } else if (*(m.id) == *(t.GetId()->WithTag(statistic::duration))) {
      // FIXME why is the error so high.
      EXPECT_NEAR(duration, m.value, 1e-8) << "Duration";
    } else {
      FAIL() << "Unexpected id " << m.id;
    }
  }
  ASSERT_TRUE(tested_something);
}

TEST(LongTaskTimerTest, Measure) {
  auto t = newTimer();

  auto task1 = t->Start();
  manual_clock.SetMonotonic(1 * 1000 * 1000 * 1000L);
  manual_clock.SetWall(123);
  assertLongTaskTimer(*t, 123, 1, 1.0);

  auto task2 = t->Start();
  assertLongTaskTimer(*t, 123, 2, 1.0);

  t->Stop(task1);
  assertLongTaskTimer(*t, 123, 1, 0.0);

  t->Stop(task2);
  assertLongTaskTimer(*t, 123, 0, 0.0);
}
