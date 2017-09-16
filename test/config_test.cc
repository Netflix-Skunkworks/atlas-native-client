#include "../util/config_manager.h"
#include <gtest/gtest.h>

using atlas::util::DefaultConfig;
using atlas::util::intern_str;
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

  atlas::meter::Tags new_tags;
  new_tags.add("foo", "bar");
  cfg->AddCommonTags(new_tags);

  auto new_common_tags = cfg->CommonTags();
  auto new_key_ref = intern_str("foo");
  auto new_val_ref = intern_str("bar");
  EXPECT_EQ(new_val_ref, new_common_tags.at(new_key_ref));
  EXPECT_EQ(orig_common_tags.size() + 1, new_common_tags.size());
}
