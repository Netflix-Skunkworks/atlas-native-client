#include "../interpreter/interpreter.h"
#include "../interpreter/all.h"
#include "../util/logger.h"
#include <cctype>
#include <gtest/gtest.h>

using atlas::util::intern_str;
using atlas::util::Logger;

using namespace atlas::interpreter;
using namespace atlas::meter;
using Strings = std::vector<const char*>;

TEST(Interpreter, SplitEmpty) {
  Expressions result;
  split("", &result);

  EXPECT_EQ(0, result.size());
  result.clear();

  split("    ", &result);
  EXPECT_EQ(0, result.size());
  result.clear();
}

void assert_tokens(const Expressions& v, const Strings& expected) {
  ASSERT_EQ(expected.size(), v.size());

  int i = 0;
  for (const auto& s : expected) {
    if (!expression::Is(*(v[i]), s)) {
      FAIL() << "Elements differ at index " << i << "- expected=" << s
             << " got=" << *(v[i]);
    }
    ++i;
  }
}

TEST(Interpreter, SplitSimple) {
  Expressions result;
  split("name,sps,:eq", &result);

  assert_tokens(result, Strings{"name", "sps", ":eq"});
  result.clear();

  split("foo", &result);
  assert_tokens(result, Strings{"foo"});
  result.clear();
}

TEST(Interpreter, SplitWithTrim) {
  Expressions result;
  split(" name ,, sps , ,   ,,:eq ", &result);

  assert_tokens(result, Strings{"name", "sps", ":eq"});
  result.clear();

  split("foo ", &result);
  assert_tokens(result, Strings{"foo"});
  result.clear();

  split(" foo ", &result);
  assert_tokens(result, Strings{"foo"});
  result.clear();
}

static Context exec(const std::string& expr) {
  Interpreter interpreter{std::make_unique<ClientVocabulary>()};
  Context context;
  interpreter.Execute(&context, expr);
  return context;
}

TEST(Interpreter, EqualQuery) {
  auto context = exec("name,sps,:eq");
  ASSERT_EQ(1, context.StackSize());

  auto expr = context.PopExpression();
  ASSERT_TRUE(expression::IsQuery(*expr));

  auto query = std::static_pointer_cast<Query>(expr);

  Tags sps{{"name", "sps"}};
  ASSERT_TRUE(query->Matches(sps));

  Tags not_sps{{"name", "sps2"}};
  ASSERT_FALSE(query->Matches(not_sps));
}

TEST(Interpreter, RegexQuery) {
  auto context = exec("id,sps,:re");
  ASSERT_EQ(1, context.StackSize());

  auto expr = context.PopExpression();
  auto query = std::static_pointer_cast<Query>(expr);

  Tags sps{{"name", "foo"}, {"id", "sps_foobar"}};
  ASSERT_TRUE(query->Matches(sps));

  Tags not_sps{{"name", "foo"}, {"id", "xsps_foobar"}};
  ASSERT_FALSE(query->Matches(not_sps));
}

static TagsValuePairs get_measurements() {
  Tags id1{{"name", "name1"}, {"k1", "v1"}};
  Tags id2{{"name", "name1"}, {"k1", "v2"}};
  Tags id3{{"name", "name1"}, {"k1", "v1"}, {"k2", "w1"}};

  TagsValuePairs res;
  res.emplace_back(TagsValuePair::of(std::move(id1), 1.0));
  res.emplace_back(TagsValuePair::of(std::move(id2), 2.0));
  res.emplace_back(TagsValuePair::of(std::move(id3), 3.0));
  return res;
}

TEST(Interpreter, All) {
  auto context = exec(":true,:all");
  ASSERT_EQ(1, context.StackSize());

  auto expr = context.PopExpression();
  ASSERT_EQ(expr->GetType(), ExpressionType::MultipleResults);

  auto all = std::static_pointer_cast<MultipleResults>(expr);

  auto measurements = get_measurements();
  auto res = all->Apply(measurements);
  EXPECT_EQ(res.size(), 3);

  Tags t1{{"name", "name1"}, {"k1", "v1"}};
  Tags t2{{"name", "name1"}, {"k1", "v2"}};
  Tags t3{{"name", "name1"}, {"k1", "v1"}, {"k2", "w1"}};
  TagsValuePairs expected{TagsValuePair::of(std::move(t1), 1.0),
                          TagsValuePair::of(std::move(t2), 2.0),
                          TagsValuePair::of(std::move(t3), 3.0)};
  EXPECT_EQ(res, expected);
}

TEST(Interpreter, AllWithQuery) {
  auto context = exec("k1,v1,:eq,:all");
  ASSERT_EQ(1, context.StackSize());

  auto expr = context.PopExpression();
  ASSERT_EQ(expr->GetType(), ExpressionType::MultipleResults);

  auto all = std::static_pointer_cast<MultipleResults>(expr);

  auto measurements = get_measurements();
  auto res = all->Apply(measurements);
  EXPECT_EQ(res.size(), 2);

  Tags t1{{"name", "name1"}, {"k1", "v1"}};
  Tags t3{{"name", "name1"}, {"k1", "v1"}, {"k2", "w1"}};
  TagsValuePairs expected{TagsValuePair::of(std::move(t1), 1.0),
                          TagsValuePair::of(std::move(t3), 3.0)};
  EXPECT_EQ(res, expected);

  std::ostringstream os;
  all->Dump(os);
  EXPECT_EQ("All(RelopQuery(k1=v1))", os.str());
}

static void test_group_by(const char* af, double expected_single,
                          double expected_multiple) {
  using atlas::interpreter::query::eq;

  std::string af_string{af};
  auto str_expression = fmt::format("name,name1,:eq,:{},(,k1,),:by", af_string);
  auto context = exec(str_expression);
  ASSERT_EQ(1, context.StackSize());

  auto expr = context.PopExpression();
  ASSERT_EQ(expr->GetType(), ExpressionType::MultipleResults);

  auto all = std::static_pointer_cast<MultipleResults>(expr);
  auto query = all->GetQuery();
  EXPECT_EQ(*eq("name", "name1"), *query);

  auto measurements = get_measurements();
  auto res = all->Apply(measurements);
  EXPECT_EQ(res.size(), 2);

  Tags t1{{"name", "name1"}, {"k1", "v1"}};
  Tags t2{{"name", "name1"}, {"k1", "v2"}};
  auto exp1 = TagsValuePair::of(std::move(t1), expected_multiple);
  auto exp2 = TagsValuePair::of(std::move(t2), expected_single);
  if (res.front()->all_tags().at(intern_str("k1")) == intern_str("v1")) {
    TagsValuePairs expected{std::move(exp1), std::move(exp2)};
    EXPECT_EQ(res, expected);
  } else {
    TagsValuePairs expected{std::move(exp2), std::move(exp1)};
    EXPECT_EQ(res, expected);
  }

  // test the string representation - mostly to keep codecov happy
  std::ostringstream os;
  all->Dump(os);
  // convert af_string to uppercase
  for (auto& c : af_string) {
    c = static_cast<char>(std::toupper(c));
  }
  auto expected_str =
      fmt::format("GroupBy(k1,{}(RelopQuery(name=name1)))", af_string);
  EXPECT_EQ(expected_str, os.str());
}

TEST(Interpreter, GroupByMax) { test_group_by("max", 2.0, 3.0); }

TEST(Interpreter, GroupBySum) { test_group_by("sum", 2.0, 4.0); }

TEST(Interpreter, GroupByMin) { test_group_by("min", 2.0, 1.0); }

TEST(Interpreter, GroupByAvg) { test_group_by("avg", 2.0, 2.0); }

TEST(Interpreter, GroupByCount) { test_group_by("count", 1.0, 2.0); }

TEST(Interpreter, ZeroCountInGroupBy) {
  auto context = exec("nf.app,foo,:eq,:count,(,k2,),:by");
  auto expr = context.PopExpression();
  auto all = std::static_pointer_cast<MultipleResults>(expr);

  auto measurements = get_measurements();
  auto res = all->Apply(measurements);
  EXPECT_EQ(res.size(), 0);
}

TEST(Interpreter, GroupByFiltering) {
  auto context = exec("name,name1,:eq,:count,(,k2,),:by");
  ASSERT_EQ(1, context.StackSize());

  auto expr = context.PopExpression();
  ASSERT_EQ(expr->GetType(), ExpressionType::MultipleResults);

  auto all = std::static_pointer_cast<MultipleResults>(expr);

  auto measurements = get_measurements();
  auto res = all->Apply(measurements);
  EXPECT_EQ(res.size(), 1);

  Tags t1{{"name", "name1"}, {"k2", "w1"}};
  TagsValuePairs expected{TagsValuePair::of(std::move(t1), 1.0)};
  EXPECT_EQ(res, expected);
}

TEST(Interpreter, KeepTags) {
  auto context = exec("name,name1,:eq,:count,(,k2,),:keep-tags");
  ASSERT_EQ(1, context.StackSize());

  auto expr = context.PopExpression();
  ASSERT_EQ(expr->GetType(), ExpressionType::MultipleResults);

  auto all = std::static_pointer_cast<MultipleResults>(expr);

  auto measurements = get_measurements();
  auto res = all->Apply(measurements);
  EXPECT_EQ(res.size(), 1);

  Tags t1{{"name", "name1"}, {"k2", "w1"}};
  TagsValuePairs expected{TagsValuePair::of(std::move(t1), 1.0)};
  EXPECT_EQ(res, expected);
}

TEST(Interpreter, DropTags) {
  auto context = exec(
      "name,name1,:eq,k2,w1,:eq,:and,:count,(,k2,does.not.exist.tag,),:drop-"
      "tags");
  ASSERT_EQ(1, context.StackSize());

  auto expr = context.PopExpression();
  ASSERT_EQ(expr->GetType(), ExpressionType::MultipleResults);

  auto all = std::static_pointer_cast<MultipleResults>(expr);

  auto measurements = get_measurements();
  Tags t1{{"name", "name1"}, {"k1", "v1"}};
  auto exp1 = TagsValuePair::of(std::move(t1), 1.0);

  auto res = all->Apply(measurements);
  TagsValuePairs expected{std::move(exp1)};
  EXPECT_EQ(expected, res);
}

TEST(Interpreter, GetQuery) {
  Interpreter interpreter{std::make_unique<ClientVocabulary>()};
  auto q = interpreter.GetQuery(":true,:all");
  EXPECT_TRUE(q->IsTrue());
}

TEST(Interpreter, InQuery) {
  auto context = exec("name,(,sps,sps2,),:in");
  ASSERT_EQ(1, context.StackSize());

  auto expr = context.PopExpression();
  ASSERT_TRUE(expression::IsQuery(*expr));

  auto query = std::static_pointer_cast<Query>(expr);

  Tags sps{{"name", "sps"}};
  ASSERT_TRUE(query->Matches(sps));

  Tags sps2{{"name", "sps2"}};
  ASSERT_TRUE(query->Matches(sps2));

  Tags sps3{{"name", "sps3"}};
  ASSERT_FALSE(query->Matches(sps3));
}

TEST(Interpreter, NestedInQuery) {
  auto context = exec("key,(,key,(,a,b,),:in,),:in");
  ASSERT_EQ(1, context.StackSize());

  auto expr = context.PopExpression();
  ASSERT_TRUE(expression::IsQuery(*expr));

  auto query = std::static_pointer_cast<Query>(expr);

  Tags a{{"key", "a"}};
  ASSERT_TRUE(query->Matches(a));

  Tags paren{{"key", ")"}};
  ASSERT_TRUE(query->Matches(paren));

  Tags in{{"key", ":in"}};
  ASSERT_TRUE(query->Matches(in));
}
