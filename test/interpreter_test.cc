#include "../interpreter/interpreter.h"
#include "../interpreter/all.h"
#include "../util/logger.h"
#include "../types.h"
#include <gtest/gtest.h>

using ::atlas::Strings;
using atlas::util::intern_str;
using atlas::util::Logger;

using namespace atlas::interpreter;
using namespace atlas::meter;

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

static Tags common_tags{{"nf.node", "i-1234"},
                        {"nf.cluster", "foo-main"},
                        {"nf.asg", "foo-main-v001"}};

static std::unique_ptr<Context> exec(const std::string& expr) {
  Interpreter interpreter{std::make_unique<ClientVocabulary>()};
  auto stack = std::make_unique<Context::Stack>();
  auto context = std::make_unique<Context>(std::move(stack));
  interpreter.Execute(context.get(), expr);
  return context;
}

TEST(Interpreter, EqualQuery) {
  auto context = exec("name,sps,:eq");
  ASSERT_EQ(1, context->StackSize());

  auto expr = context->PopExpression();
  ASSERT_TRUE(expression::IsQuery(*expr));

  auto query = std::static_pointer_cast<Query>(expr);

  Tags sps{{"name", "sps"}};
  ASSERT_TRUE(query->Matches(sps));

  Tags not_sps{{"name", "sps2"}};
  ASSERT_FALSE(query->Matches(not_sps));
}

TEST(Interpreter, RegexQuery) {
  auto context = exec("id,sps,:re");
  ASSERT_EQ(1, context->StackSize());

  auto expr = context->PopExpression();
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

  TagsValuePair m1{id1, 1.0};
  TagsValuePair m2{id2, 2.0};
  TagsValuePair m3{id3, 3.0};

  return TagsValuePairs{m1, m2, m3};
}

TEST(Interpreter, All) {
  auto context = exec(":true,:all");
  ASSERT_EQ(1, context->StackSize());

  auto expr = context->PopExpression();
  ASSERT_EQ(expr->GetType(), ExpressionType::MultipleResults);

  auto all = std::static_pointer_cast<MultipleResults>(expr);

  auto measurements = get_measurements();
  auto res = all->Apply(measurements);
  EXPECT_EQ(res.size(), 3);

  Tags t1{{"name", "name1"}, {"k1", "v1"}};
  Tags t2{{"name", "name1"}, {"k1", "v2"}};
  Tags t3{{"name", "name1"}, {"k1", "v1"}, {"k2", "w1"}};
  TagsValuePair exp1{t1, 1.0};
  TagsValuePair exp2{t2, 2.0};
  TagsValuePair exp3{t3, 3.0};
  TagsValuePairs expected{exp1, exp2, exp3};
  EXPECT_EQ(res, expected);
}

TEST(Interpreter, GroupBy) {
  auto context = exec("name,name1,:eq,:sum,(,k1,),:by");
  ASSERT_EQ(1, context->StackSize());

  auto expr = context->PopExpression();
  ASSERT_EQ(expr->GetType(), ExpressionType::MultipleResults);

  auto all = std::static_pointer_cast<MultipleResults>(expr);

  auto measurements = get_measurements();
  auto res = all->Apply(measurements);
  EXPECT_EQ(res.size(), 2);

  Tags t1{{"name", "name1"}, {"k1", "v1"}};
  Tags t2{{"name", "name1"}, {"k1", "v2"}};
  TagsValuePair exp1{t1, 4.0};
  TagsValuePair exp2{t2, 2.0};
  if (res.at(0).tags.at(intern_str("k1")) == intern_str("v1")) {
    TagsValuePairs expected{exp1, exp2};
    EXPECT_EQ(res, expected);
  } else {
    TagsValuePairs expected{exp2, exp1};
    EXPECT_EQ(res, expected);
  }
}

TEST(Interpreter, GroupByFiltering) {
  auto context = exec("name,name1,:eq,:count,(,k2,),:by");
  ASSERT_EQ(1, context->StackSize());

  auto expr = context->PopExpression();
  ASSERT_EQ(expr->GetType(), ExpressionType::MultipleResults);

  auto all = std::static_pointer_cast<MultipleResults>(expr);

  auto measurements = get_measurements();
  auto res = all->Apply(measurements);
  EXPECT_EQ(res.size(), 1);

  Tags t1{{"name", "name1"}, {"k2", "w1"}};
  TagsValuePair exp1{t1, 1.0};
  TagsValuePairs expected{exp1};
  EXPECT_EQ(res, expected);
}

TEST(Interpreter, KeepTags) {
  auto context = exec("name,name1,:eq,:count,(,k2,),:keep-tags");
  ASSERT_EQ(1, context->StackSize());

  auto expr = context->PopExpression();
  ASSERT_EQ(expr->GetType(), ExpressionType::MultipleResults);

  auto all = std::static_pointer_cast<MultipleResults>(expr);

  auto measurements = get_measurements();
  auto res = all->Apply(measurements);
  EXPECT_EQ(res.size(), 1);

  Tags t1{{"name", "name1"}, {"k2", "w1"}};
  TagsValuePair exp1{t1, 1.0};
  TagsValuePairs expected{exp1};
  EXPECT_EQ(res, expected);
}

TEST(Interpreter, DropTags) {
  auto context = exec("name,name1,:eq,k2,w1,:eq,:and,:count,(,k2,),:drop-tags");
  ASSERT_EQ(1, context->StackSize());

  auto expr = context->PopExpression();
  ASSERT_EQ(expr->GetType(), ExpressionType::MultipleResults);

  auto all = std::static_pointer_cast<MultipleResults>(expr);

  auto measurements = get_measurements();
  for (const auto& m : measurements) {
    Logger()->info("M: {}", m);
  }
  auto res = all->Apply(measurements);
  EXPECT_EQ(res.size(), 2);

  Tags t1{{"name", "name1"}, {"k1", "v1"}};
  Tags t2{{"name", "name1"}, {"k1", "v2"}};
  TagsValuePair exp1{t1, 1.0};
  TagsValuePair exp2{t2, 0.0};
  EXPECT_EQ(res.size(), 2);
  if (res.at(0).tags == t1) {
    TagsValuePairs expected{exp1, exp2};
    EXPECT_EQ(expected, res);
  } else {
    TagsValuePairs expected{exp2, exp1};
    EXPECT_EQ(expected, res);
  }
}

TEST(Interpreter, GetQuery) {
  Interpreter interpreter{std::make_unique<ClientVocabulary>()};
  auto q = interpreter.GetQuery(":true,:all");
  EXPECT_TRUE(q->IsTrue());
}