#include "../meter/validation.h"
#include "../util/logger.h"
#include <gtest/gtest.h>

using atlas::meter::AnalyzeTags;
using atlas::meter::AreTagsValid;
using atlas::meter::Tags;
using atlas::meter::ValidationIssue;
using atlas::util::intern_str;

TEST(Validation, NoName) {
  Tags empty;
  EXPECT_FALSE(AreTagsValid(empty));

  Tags some_random_tags{{"k1", "v1"}, {"k2", "v2"}};
  EXPECT_FALSE(AreTagsValid(some_random_tags));

  Tags valid{{"name", "foobar"}};
  EXPECT_TRUE(AreTagsValid(valid));
}

TEST(Validation, LongName) {
  std::string just_right(255, 'x');
  Tags just_right_name{{"name", just_right.c_str()}};
  EXPECT_TRUE(AreTagsValid(just_right_name));

  std::string too_long(256, 'x');
  Tags too_long_name{{"name", too_long.c_str()}};
  EXPECT_FALSE(AreTagsValid(too_long_name));
}

TEST(Validation, KeyLengths) {
  std::string key_just_right(60, 'k');
  Tags just_right{{"name", "foo"}, {key_just_right.c_str(), "x"}};
  EXPECT_TRUE(AreTagsValid(just_right));

  std::string key_too_long(61, 'k');
  Tags too_long{{"name", "foo"}, {key_too_long.c_str(), "v"}};
  EXPECT_FALSE(AreTagsValid(too_long));
}

TEST(Validation, EmptyKeyOrValue) {
  Tags empty_name{{"name", ""}, {"k", "v"}, {"k2", "v2"}};
  EXPECT_FALSE(AreTagsValid(empty_name));

  Tags empty_key{{"name", "n"}, {"", "v"}, {"k2", "v"}};
  EXPECT_FALSE(AreTagsValid(empty_key));

  Tags empty_value{{"name", "n"}, {"k1", ""}, {"k2", ""}, {"k3", "v"}};
  EXPECT_FALSE(AreTagsValid(empty_value));
}

TEST(Validation, InvalidKey) {
  Tags uses_atlas_ns{
      {"name", "n"}, {"atlas.rules", "true"}, {"atlas.legacy", "epic"}};
  EXPECT_FALSE(AreTagsValid(uses_atlas_ns));

  Tags uses_nf_ns{{"name", "n"}, {"nf.country", "us"}, {"nf.legacy", "epic"}};
  EXPECT_FALSE(AreTagsValid(uses_nf_ns));

  Tags uses_valid_atlas{{"atlas.dstype", "counter"}, {"name", "n"}};
  EXPECT_TRUE(AreTagsValid(uses_valid_atlas));

  Tags uses_valid_nf{{"nf.country", "ar"}, {"name", "n"}};
  EXPECT_TRUE(AreTagsValid(uses_valid_nf));
}

TEST(Validation, TooManyUserTags) {
  Tags tags = {
      {"name", "n"}, {"atlas.dstype", "counter"}, {"nf.country", "us"}};

  for (auto i = 1; i < 20; ++i) {
    tags.add(std::to_string(i).c_str(), "v");
  }
  EXPECT_TRUE(AreTagsValid(tags));

  tags.add("extra", "v");
  EXPECT_FALSE(AreTagsValid(tags));
}

TEST(ValidationA, NoName) {
  Tags empty;
  auto res = AnalyzeTags(empty);
  EXPECT_EQ(res.size(), 1);

  Tags some_random_tags{{"k1", "v1"}, {"k2", "v2"}};
  res = AnalyzeTags(some_random_tags);
  EXPECT_EQ(res.size(), 1);

  Tags valid{{"name", "foobar"}};
  EXPECT_TRUE(AnalyzeTags(valid).empty());
}

TEST(ValidationA, LongName) {
  std::string just_right(255, 'x');
  Tags just_right_name{{"name", just_right.c_str()}};
  EXPECT_TRUE(AnalyzeTags(just_right_name).empty());

  std::string too_long(256, 'x');
  Tags too_long_name{{"name", too_long.c_str()}};
  auto res = AnalyzeTags(too_long_name);
  EXPECT_EQ(res.size(), 1);
}

TEST(ValidationA, KeyLengths) {
  std::string key_just_right(60, 'k');
  Tags just_right{{"name", "foo"}, {key_just_right.c_str(), "x"}};
  EXPECT_TRUE(AnalyzeTags(just_right).empty());

  std::string key_too_long(61, 'k');
  Tags too_long{{"name", "foo"}, {key_too_long.c_str(), "v"}};
  auto res = AnalyzeTags(too_long);
  EXPECT_EQ(res.size(), 1);
}

TEST(ValidationA, EmptyKeyOrValue) {
  Tags empty_name{{"name", ""}, {"k", "v"}, {"k2", "v2"}};
  auto res = AnalyzeTags(empty_name);
  EXPECT_EQ(res.size(), 1);
  Tags empty_key{{"name", "n"}, {"", "v"}, {"k2", "v"}};
  res = AnalyzeTags(empty_key);
  EXPECT_EQ(res.size(), 1);

  Tags empty_value{{"name", "n"}, {"k1", ""}, {"k2", ""}, {"k3", "v"}};
  res = AnalyzeTags(empty_value);
  EXPECT_EQ(res.size(), 1);
}

TEST(ValidationA, InvalidKey) {
  Tags uses_atlas_ns{
      {"name", "n"}, {"atlas.rules", "true"}, {"atlas.legacy", "epic"}};
  auto res = AnalyzeTags(uses_atlas_ns);
  EXPECT_EQ(res.size(), 1);

  Tags uses_nf_and_atlas_ns{{"name", "n"},
                            {"nf.country", "us"},
                            {"nf.legacy", "epic"},
                            {"atlas.x", "foo"}};
  res = AnalyzeTags(uses_nf_and_atlas_ns);
  EXPECT_EQ(res.size(), 2);

  Tags uses_valid_atlas{{"atlas.dstype", "counter"}, {"name", "n"}};
  EXPECT_TRUE(AnalyzeTags(uses_valid_atlas).empty());

  Tags uses_valid_nf{{"nf.country", "ar"}, {"name", "n"}};
  EXPECT_TRUE(AnalyzeTags(uses_valid_nf).empty());
}

TEST(ValidationA, TooManyUserTags) {
  Tags tags = {
      {"name", "n"}, {"atlas.dstype", "counter"}, {"nf.country", "us"}};

  for (auto i = 1; i < 20; ++i) {
    tags.add(std::to_string(i).c_str(), "v");
  }
  EXPECT_TRUE(AnalyzeTags(tags).empty());

  tags.add("extra", "v");
  auto res = AnalyzeTags(tags);
  EXPECT_EQ(res.size(), 1);
  for (auto& issue : res) {
    atlas::util::Logger()->info("{}", issue.ToString());
  }
}

TEST(ValidationA, Warnings) {
  Tags tags = {{"name", "n~1"},
               {"nf.asg", "foo~1"},
               {"atlas.dstype", "foo bar"},
               {"nf.country", "us#1"},
               {"blah ", "bar"}};

  auto res = AnalyzeTags(tags);
  EXPECT_EQ(res.size(), 4);
  for (auto& issue : res) {
    EXPECT_EQ(issue.level, ValidationIssue::Level::WARN);
    atlas::util::Logger()->info("{}", issue.ToString());
  }
}
