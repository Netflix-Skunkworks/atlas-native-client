#include "../util/strings.h"
#include <gtest/gtest.h>

using namespace atlas::util;
using std::string;

static atlas::util::StrRef to_ref(const char* s) { return intern_str(s); }

TEST(ExpandVars, InPlace) {
  string tmpl = "foo $bar ${baz}";
  auto replacer = [](const string& var) -> string {
    EXPECT_TRUE(var == "bar" || var == "baz")
        << "Unexpected '" << var << "' variable";
    if (var == "bar") {
      return "BAR";
    }
    if (var == "baz") {
      return "XBAZ";
    }
    return "";
  };

  EXPECT_EQ(ExpandVars(tmpl, replacer), "foo BAR XBAZ");
}

TEST(ExpandVars, Words) {
  string tmpl = "$baz.iep$bar.$baz.netflix";
  auto replacer = [](const string& var) -> string {
    EXPECT_TRUE(var == "bar" || var == "baz")
        << "Unexpected '" << var << "' variable";
    if (var == "bar") {
      return "BAR";
    }
    if (var == "baz") {
      return "XBAZ";
    }
    return "";
  };

  EXPECT_EQ(ExpandVars(tmpl, replacer), "XBAZ.iepBAR.XBAZ.netflix");
}

TEST(Strings, ToValidCharset) {
  auto valid = to_ref("foo_bar");
  EXPECT_EQ(valid, ToValidCharset(valid));

  auto invalid = to_ref("bar/foo");
  EXPECT_EQ(ToValidCharset(invalid), to_ref("bar_foo"));

  auto empty = to_ref("");
  EXPECT_EQ(ToValidCharset(empty), to_ref(""));
}

TEST(Strings, EncodeValueForKey) {
  EXPECT_EQ(to_ref("abc.1234"),
            EncodeValueForKey(to_ref("abc.1234"), to_ref("nf.asg")))
      << "No encoding needed";
  EXPECT_EQ(to_ref("abc_abc"),
            EncodeValueForKey(to_ref("abc_abc"), to_ref("nf.asg")))
      << "Does not encode underscores";
  EXPECT_EQ(to_ref("^abcabc"),
            EncodeValueForKey(to_ref("^abcabc"), to_ref("nf.asg")))
      << "Caret allowed for nf.asg";
  EXPECT_EQ(to_ref("abcabc_"),
            EncodeValueForKey(to_ref("abcabc/"), to_ref("nf.cluster")))
      << "Special char at end";
  EXPECT_EQ(to_ref("abc_abc_"),
            EncodeValueForKey(to_ref("abc{abc}"), to_ref("nf.cluster")))
      << "Multiple substitutions";
  EXPECT_EQ(to_ref("abc^abc~_"),
            EncodeValueForKey(to_ref("abc^abc~#"), to_ref("nf.cluster")))
      << "Multiple substitutions, plus invalid";
  EXPECT_EQ(to_ref("abc_abc__"),
            EncodeValueForKey(to_ref("abc^abc~#"), to_ref("nf.app")))
      << "Multiple substitutions for normal keys";
}

TEST(Strings, JoinPath) {
  EXPECT_EQ("./foo.log", join_path("", "foo.log"));
  EXPECT_EQ("dir/foo.log", join_path("dir", "foo.log"));
  EXPECT_EQ("a/foo.log", join_path("a/", "foo.log"));
  EXPECT_EQ("a/foo.log", join_path("a", "foo.log"));
}

TEST(Strings, TrimRight) {
  std::string empty("");
  TrimRight(&empty);
  EXPECT_EQ("", empty);

  std::string no_need(" foo");
  TrimRight(&no_need);
  EXPECT_EQ(" foo", no_need);

  std::string some_blanks("foo \t \n");
  TrimRight(&some_blanks);
  EXPECT_EQ("foo", some_blanks);

  std::string just_one("foo\n");
  TrimRight(&just_one);
  EXPECT_EQ("foo", just_one);
}

TEST(Strings, ToLowerCopy) {
  std::string mixed{"The Quick!123"};
  EXPECT_EQ(ToLowerCopy(mixed), "the quick!123");
}

TEST(Strings, ExpandVarsUrl) {
  const char* url =
      "http://"
      "atlas-pub-$EC2_OWNER_ID.$EC2_REGION.iep$NETFLIX_ENVIRONMENT.netflix.net/"
      "api/v1/publish-fast";
  std::map<std::string, std::string> env = {{"EC2_OWNER_ID", "179727101194"},
                                            {"NETFLIX_ENVIRONMENT", "test"},
                                            {"EC2_REGION", "us-east-1"}};
  auto replacer = [&env](const string& var) -> string {
    EXPECT_TRUE(env.find(var) != env.end())
        << "Unknown variable '" << var << "'";
    return env[var];
  };

  EXPECT_EQ(ExpandVars(url, replacer),
            "http://atlas-pub-179727101194.us-east-1.ieptest.netflix.net/api/"
            "v1/publish-fast");
}