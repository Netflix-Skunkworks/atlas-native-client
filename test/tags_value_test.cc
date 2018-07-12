#include "gtest/gtest.h"
#include "../interpreter/tags_value.h"
#include "../meter/id_format.h"

using atlas::interpreter::IdTagsValuePair;
using atlas::interpreter::SimpleTagsValuePair;
using atlas::interpreter::TagsValuePair;
using atlas::meter::Id;
using atlas::meter::Tags;
using atlas::util::intern_str;

TEST(SimpleTagsValuePair, get_value) {
  Tags t{{"k1", "v1"}, {"k2", "v2"}};
  SimpleTagsValuePair tv{std::move(t), 42.0};

  auto v1 = tv.get_value(intern_str("k1"));
  EXPECT_TRUE(v1);
  EXPECT_EQ(v1, "v1");

  auto v2 = tv.get_value(intern_str("k2"));
  EXPECT_TRUE(v2);
  EXPECT_EQ(v2, "v2");

  EXPECT_FALSE(tv.get_value(intern_str("foo")));
  EXPECT_EQ(tv.value(), 42.0);
}

TEST(SimpleTagsValuePair, all_tags) {
  Tags t{{"k1", "v1"}, {"k2", "v2"}};
  Tags t_copy{t};
  SimpleTagsValuePair tv{std::move(t), 42.0};
  EXPECT_EQ(t_copy, tv.all_tags());
}

IdTagsValuePair get_id_tv_pair() {
  static auto common_tags = Tags{{"nf.foo", "foo-main"}};
  auto id = std::make_shared<Id>("n", Tags{{"k1", "v1"}});
  return IdTagsValuePair{id, &common_tags, 42.0};
}

TEST(IdTagsValuePair, get_value) {
  auto tv = get_id_tv_pair();

  auto n = tv.get_value(intern_str("name"));
  EXPECT_EQ(n, "n");

  auto nf_foo = tv.get_value(intern_str("nf.foo"));
  EXPECT_EQ(nf_foo, "foo-main");

  auto k = tv.get_value(intern_str("k1"));
  EXPECT_EQ(k, "v1");

  EXPECT_FALSE(tv.get_value(intern_str("foo")));

  EXPECT_EQ(tv.value(), 42.0);
}

TEST(IdTagsValuePair, all_tags) {
  auto tv = get_id_tv_pair();

  Tags expected{{"nf.foo", "foo-main"}, {"name", "n"}, {"k1", "v1"}};
  EXPECT_EQ(tv.all_tags(), expected);
}

static std::string to_str(const TagsValuePair& tv) {
  std::ostringstream os;
  os << tv;
  return os.str();
}

static std::string to_str(const std::shared_ptr<TagsValuePair>& tv) {
  std::ostringstream os;
  os << tv;
  return os.str();
}

TEST(SimpleTagsValuePair, ToString) {
  Tags t{{"k1", "v1"}, {"k2", "v2"}};
  Tags t2{t};
  SimpleTagsValuePair tv{std::move(t), 1.0};
  auto tags_str = fmt::format("{}", t2);
  EXPECT_EQ(to_str(tv),
            fmt::format("SimpleTagsValuePair(tags={},value=1)", tags_str));

  auto ptr_tv = std::make_shared<SimpleTagsValuePair>(std::move(t2), 2.0);
  EXPECT_EQ(to_str(ptr_tv),
            fmt::format("SimpleTagsValuePair(tags={},value=2)", tags_str));
}

TEST(IdTagsValuePair, ToString) {
  auto tv = get_id_tv_pair();
  EXPECT_EQ(to_str(tv),
            "IdTagsValuePair(id=Id(n, "
            "[k1->v1]),common_tags=[nf.foo->foo-main],value=42)");
}
