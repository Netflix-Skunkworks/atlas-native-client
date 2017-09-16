#include "../meter/validation.h"
#include <gtest/gtest.h>

using atlas::meter::Tags;
using atlas::meter::validation::IsValid;
using atlas::util::intern_str;

static Tags from(std::map<std::string, std::string> ts) {
  Tags res;
  for (const auto& kv : ts) {
    res.add(intern_str(kv.first), intern_str(kv.second));
  }
  return res;
}

TEST(Validation, NoName) {
  Tags empty;
  EXPECT_FALSE(IsValid(empty));

  Tags some_random_tags = from({{"k1", "v1"}, {"k2", "v2"}});
  EXPECT_FALSE(IsValid(some_random_tags));

  Tags valid = from({{"name", "foobar"}});
  EXPECT_TRUE(IsValid(valid));
}

TEST(Validation, LongName) {
  std::string just_right(255, 'x');
  Tags just_right_name = from({{"name", just_right}});
  EXPECT_TRUE(IsValid(just_right_name));

  std::string too_long(256, 'x');
  Tags too_long_name = from({{"name", too_long}});
  EXPECT_FALSE(IsValid(too_long_name));
}

TEST(Validation, KeyLengths) {
  std::string key_just_right(60, 'k');
  Tags just_right = from({{"name", "foo"}, {key_just_right, "x"}});
  EXPECT_TRUE(IsValid(just_right));

  std::string key_too_long(61, 'k');
  Tags too_long = from({{"name", "foo"}, {key_too_long, "v"}});
  EXPECT_FALSE(IsValid(too_long));
}

TEST(Validation, EmptyKeyOrValue) {
  Tags empty_name = from({{"name", ""}, {"k", "v"}, {"k2", "v2"}});
  EXPECT_FALSE(IsValid(empty_name));

  Tags empty_key = from({{"name", "n"}, {"", "v"}, {"k2", "v"}});
  EXPECT_FALSE(IsValid(empty_key));

  Tags empty_value = from({{"name", "n"}, {"k1", ""}, {"k2", ""}, {"k3", "v"}});
  EXPECT_FALSE(IsValid(empty_value));
}

TEST(Validation, InvalidKey) {
  Tags uses_atlas_ns =
      from({{"name", "n"}, {"atlas.rules", "true"}, {"atlas.legacy", "epic"}});
  EXPECT_FALSE(IsValid(uses_atlas_ns));

  Tags uses_nf_ns =
      from({{"name", "n"}, {"nf.country", "us"}, {"nf.legacy", "epic"}});
  EXPECT_FALSE(IsValid(uses_nf_ns));

  Tags uses_valid_atlas = from({{"atlas.dstype", "counter"}, {"name", "n"}});
  EXPECT_TRUE(IsValid(uses_valid_atlas));

  Tags uses_valid_nf = from({{"nf.country", "ar"}, {"name", "n"}});
  EXPECT_TRUE(IsValid(uses_valid_nf));
}

TEST(Validation, TooManyUserTags) {
  Tags tags =
      from({{"name", "n"}, {"atlas.dstype", "counter"}, {"nf.country", "us"}});

  for (auto i = 1; i < 20; ++i) {
    tags.add(std::to_string(i).c_str(), "v");
  }
  EXPECT_TRUE(IsValid(tags));

  tags.add("extra", "v");
  EXPECT_FALSE(IsValid(tags));
}