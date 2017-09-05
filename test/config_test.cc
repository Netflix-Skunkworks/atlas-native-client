#include "../util/config_manager.h"
#include <gtest/gtest.h>

using atlas::util::DefaultConfig;
using std::make_pair;
using std::map;
using std::string;

TEST(Config, LoggingDirectory) {
  auto cfg = DefaultConfig();
  // running tests we set the logger to stderr, so it should be empty.
  EXPECT_TRUE(cfg->LoggingDirectory().empty());
}

TEST(Config, AddCommonTag) {
  auto cfg = DefaultConfig();
  auto orig_common_tags = cfg->CommonTags();

  map<string, string> new_tags;
  string new_key = "foo";
  string new_val = "bar";
  new_tags.emplace(make_pair(new_key, new_val));
  cfg->AddCommonTags(new_tags);

  auto new_common_tags = cfg->CommonTags();
  EXPECT_EQ(new_val, new_common_tags[new_key]);
  EXPECT_EQ(orig_common_tags.size() + 1, new_common_tags.size());
}
