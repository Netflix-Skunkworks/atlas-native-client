#include "../util/small_tag_map.h"
#include <gtest/gtest.h>

using atlas::util::SmallTagMap;

TEST(SmallTagMap, Init) {
  SmallTagMap map;
  EXPECT_EQ(map.size(), 0);
}

TEST(SmallTagMap, Add) {
  SmallTagMap map;
  map.add("test", "value");
  map.add("test", "value1");
  map.add("test", "value2");
  EXPECT_EQ(map.size(), 1);
  char buf[32];
  for (auto i = 0; i < 20; ++i) {
    snprintf(buf, sizeof buf, "test%d", i);
    map.add(buf, buf);
    EXPECT_EQ(map.size(), i + 2);
  }
}

TEST(SmallTagMap, Get) {
  using atlas::util::intern_str;

  SmallTagMap map;
  auto empty = intern_str("");
  auto test_ref = intern_str("test");
  EXPECT_EQ(map.at(test_ref).get(), empty.get()) << "at on empty map";

  map.add("test", "value");
  EXPECT_EQ(map.at(test_ref).get(), intern_str("value").get()) << "simple get";

  map.add("test", "value1");
  map.add("test", "value2");
  EXPECT_EQ(map.at(test_ref).get(), intern_str("value2").get()) << "simple get";

  char buf[32];
  for (auto i = 0; i < 20; ++i) {
    snprintf(buf, sizeof buf, "test%d", i);
    map.add(buf, buf);
    auto buf_ref = intern_str(buf);
    EXPECT_EQ(map.at(buf_ref).get(), buf_ref.get());
  }
}
