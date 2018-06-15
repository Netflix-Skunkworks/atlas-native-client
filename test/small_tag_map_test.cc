#include "../util/small_tag_map.h"
#include <gtest/gtest.h>

using atlas::util::SmallTagMap;

static void add(SmallTagMap* map, const char* k, const char* v) {
  map->add(atlas::util::intern_str(k), atlas::util::intern_str(v));
}

TEST(SmallTagMap, Init) {
  SmallTagMap map;
  EXPECT_EQ(map.size(), 0);
}

TEST(SmallTagMap, Add) {
  SmallTagMap map;
  add(&map, "test", "value");
  add(&map, "test", "value1");
  add(&map, "test", "value2");
  EXPECT_EQ(map.size(), 1);
  char buf[32];
  for (auto i = 0; i < 20; ++i) {
    snprintf(buf, sizeof buf, "test%d", i);
    add(&map, buf, buf);
    EXPECT_EQ(map.size(), i + 2);
  }
}

TEST(SmallTagMap, Get) {
  using atlas::util::intern_str;

  SmallTagMap map;
  auto test_ref = intern_str("test");
  EXPECT_TRUE(map.at(test_ref).is_null()) << "at on empty map";

  add(&map, "test", "value");
  EXPECT_EQ(map.at(test_ref).get(), intern_str("value").get()) << "simple get";

  add(&map, "test", "value1");
  add(&map, "test", "value2");
  EXPECT_EQ(map.at(test_ref).get(), intern_str("value2").get()) << "simple get";

  char buf[32];
  for (auto i = 0; i < 20; ++i) {
    snprintf(buf, sizeof buf, "test%d", i);
    add(&map, buf, buf);
    auto buf_ref = intern_str(buf);
    EXPECT_EQ(map.at(buf_ref).get(), buf_ref.get());
  }
}

TEST(SmallTagMap, AddAll) {
  auto map1 = SmallTagMap{{"k1", "v1"}, {"k2", "v2"}};
  auto map2 = SmallTagMap{{"k3", "v1"}, {"k4", "v2"}};
  auto expected =
      SmallTagMap{{"k1", "v1"}, {"k2", "v2"}, {"k3", "v1"}, {"k4", "v2"}};

  map1.add_all(map2);
  EXPECT_EQ(map1, expected);
}

TEST(SmallTagMap, AddAllOverrides) {
  auto map1 = SmallTagMap{{"k1", "v1"}, {"k2", "v2"}};
  auto map2 = SmallTagMap{{"k3", "v3"}, {"k1", "v4"}};
  auto expected = SmallTagMap{{"k1", "v4"}, {"k2", "v2"}, {"k3", "v3"}};

  map1.add_all(map2);
  EXPECT_EQ(map1, expected);
}

TEST(SmallTagMap, CopyConstructor) {
  SmallTagMap common_tags;
  add(&common_tags, "nf.app", "foo");
  add(&common_tags, "nf.cluster", "foo-main");
  add(&common_tags, "nf.x", "foo");
  add(&common_tags, "nf.y", "foo");
  add(&common_tags, "nf.z", "foo");
  add(&common_tags, "nf.a", "foo");
  add(&common_tags, "nf.b", "foo");
  add(&common_tags, "nf.c", "foo");
  add(&common_tags, "nf.d", "foo");
  add(&common_tags, "nf.e", "foo");
  add(&common_tags, "nf.f", "foo");
  add(&common_tags, "nf.g", "foo");
  SmallTagMap copy{common_tags};
  EXPECT_EQ(copy, common_tags);
}
