#include "../meter/validation.h"
#include <gtest/gtest.h>

using atlas::meter::Tags;
using atlas::meter::validation::IsValid;

TEST(Validation, NoName) {
  Tags empty;
  EXPECT_FALSE(IsValid(empty));

  Tags some_random_tags{{"k1", "v1"}, {"k2", "v2"}};
  EXPECT_FALSE(IsValid(some_random_tags));

  Tags valid{{"name", "foobar"}};
  EXPECT_TRUE(IsValid(valid));
}

TEST(Validation, LongName) {
  std::string just_right(255, 'x');
  Tags just_right_name{{"name", just_right}};
  EXPECT_TRUE(IsValid(just_right_name));

  std::string too_long(256, 'x');
  Tags too_long_name{{"name", too_long}};
  EXPECT_FALSE(IsValid(too_long_name));
}

TEST(Validation, KeyLengths) {
  std::string key_just_right(60, 'k');
  Tags just_right{{"name", "foo"}, {key_just_right, "x"}};
  EXPECT_TRUE(IsValid(just_right));

  std::string key_too_long(61, 'k');
  Tags too_long{{"name", "foo"}, {key_too_long, "v"}};
  EXPECT_FALSE(IsValid(too_long));
}

TEST(Validation, EmptyKeyOrValue) {
  Tags empty_name{{"name", ""}, {"k", "v"}, {"k2", "v2"}};
  EXPECT_FALSE(IsValid(empty_name));

  Tags empty_key{{"name", "n"}, {"", "v"}, {"k2", "v"}};
  EXPECT_FALSE(IsValid(empty_key));

  Tags empty_value{{"name", "n"}, {"k1", ""}, {"k2", ""}, {"k3", "v"}};
  EXPECT_FALSE(IsValid(empty_value));
}

TEST(Validation, InvalidKey) {
  Tags uses_atlas_ns{
      {"name", "n"}, {"atlas.rules", "true"}, {"atlas.legacy", "epic"}};
  EXPECT_FALSE(IsValid(uses_atlas_ns));

  Tags uses_nf_ns{{"name", "n"}, {"nf.country", "us"}, {"nf.legacy", "epic"}};
  EXPECT_FALSE(IsValid(uses_nf_ns));

  Tags uses_valid_atlas{{"atlas.dstype", "counter"}, {"name", "n"}};
  EXPECT_TRUE(IsValid(uses_valid_atlas));

  Tags uses_valid_nf{{"nf.country", "ar"}, {"name", "n"}};
  EXPECT_TRUE(IsValid(uses_valid_nf));
}

TEST(Validation, TooManyUserTags) {
  Tags tags{{"name", "n"}, {"atlas.dstype", "counter"}, {"nf.country", "us"}};

  for (auto i = 1; i < 20; ++i) {
    tags[std::to_string(i)] = "v";
  }
  EXPECT_TRUE(IsValid(tags));

  tags["extra"] = "v";
  EXPECT_FALSE(IsValid(tags));
}