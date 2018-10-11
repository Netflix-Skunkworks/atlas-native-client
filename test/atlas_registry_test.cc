#include "../interpreter/interpreter.h"
#include "../meter/atlas_registry.h"
#include "../meter/manual_clock.h"
#include "../util/config_manager.h"
#include "../util/logger.h"
#include "test_registry.h"
#include <gtest/gtest.h>

using atlas::util::Config;
using atlas::util::ConfigManager;
using atlas::util::intern_str;
using atlas::util::Logger;

using atlas::interpreter::ClientVocabulary;
using atlas::interpreter::Interpreter;
using atlas::interpreter::TagsValuePair;
using atlas::interpreter::TagsValuePairs;
using namespace atlas::meter;

TEST(AtlasRegistry, MetersExpire) {
  ManualClock manual_clock;
  TestRegistry r{60000, &manual_clock};
  r.SetWall(42);
  Tags tags{{"k1", "v1"}, {"k2", "v2"}};
  auto id1 = r.CreateId("m1", tags);
  auto id3 = r.CreateId("m3", tags);
  Tags tags_for_4{{"k1", "v1"}, {"k2", "v3"}};
  auto id4 = r.CreateId("m3", tags_for_4);

  auto ctr = r.counter(id1);
  ctr->Increment();
  // make sure we re-use ids
  r.counter(r.CreateId("m2", tags))->Add(60);
  r.counter(r.CreateId("m2", tags))->Add(60);
  r.counter(id3)->Add(60);
  r.counter(id4)->Add(60);

  {
    auto ms = r.my_meters();
    EXPECT_EQ(ms.size(), 4);
  }

  r.SetWall(atlas::meter::MAX_IDLE_TIME + 43);
  r.expire();
  auto ms = r.my_meters();
  EXPECT_EQ(ms.size(), 1);
}

TEST(AtlasRegistry, MeasurementsExpire) {
  ManualClock manual_clock;
  TestRegistry r{60000, &manual_clock};
  r.SetWall(42);
  Tags tags{{"k1", "v1"}, {"k2", "v2"}};
  auto id1 = r.CreateId("m1", tags);
  auto id3 = r.CreateId("m3", tags);
  Tags tags_for_4{{"k1", "v1"}, {"k2", "v3"}};
  auto id4 = r.CreateId("m3", tags_for_4);

  auto ctr = r.counter(id1);
  ctr->Increment();
  // make sure we re-use ids
  r.counter(r.CreateId("m2", tags))->Add(60);
  r.counter(r.CreateId("m2", tags))->Add(60);
  r.counter(id3)->Add(60);
  r.counter(id4)->Add(60);

  r.measurements();
  r.SetWall(atlas::meter::MAX_IDLE_TIME + 43);
  r.measurements();
  auto meters = r.my_meters();
  EXPECT_EQ(meters.size(), 1);
}

TEST(AtlasRegistry, DiffTypes) {
  ManualClock manual_clock;
  TestRegistry r{60000, &manual_clock};

  auto id = r.CreateId("foo", {{"statistic", "test"}});
  r.timer(id)->Record(21);
  r.timer(id)->Record(21);

  r.counter(id)->Increment();
  auto c = r.counter(id);
  c->Increment();

  EXPECT_EQ(c->Count(), 1);  // this is an isolated object
  EXPECT_EQ(r.timer(id)->TotalTime(),
            42);  // the original is still in the registry
}