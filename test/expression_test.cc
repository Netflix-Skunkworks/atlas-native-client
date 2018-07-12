#include "gtest/gtest.h"
#include "../interpreter/expression.h"
#include "../interpreter/singleton_value_expr.h"
#include "test_utils.h"

using namespace atlas::interpreter;
using atlas::meter::Tags;
using atlas::util::intern_str;

TEST(ConstantExpression, Basic) {
  ConstantExpression ce{5.0};
  auto res = ce.Apply(TagsValuePairs{});
  auto expected = TagsValuePair::of(Tags{}, 5.0);
  EXPECT_EQ(*expected, *res);
  EXPECT_EQ("ConstantExpression(5)", to_str(ce));
  EXPECT_EQ(ce.GetType(), ExpressionType::ValueExpression);
  EXPECT_TRUE(ce.GetQuery()->IsFalse());
}

TEST(SingletonValueExpr, Basic) {
  auto ve = std::make_shared<ConstantExpression>(5.0);
  auto sve = SingletonValueExpr(ve);
  EXPECT_EQ(sve.GetType(), ExpressionType::MultipleResults);
  EXPECT_EQ("SingletonVE{expr=ConstantExpression(5)}", to_str(sve));
  auto res = sve.Apply(TagsValuePairs{});
  auto expected = TagsValuePairs{TagsValuePair::of(Tags{}, 5.0)};
  EXPECT_EQ(expected, res);
  EXPECT_TRUE(sve.GetQuery()->IsFalse());
}

TEST(List, Basic) {
  List list;
  list.Add(std::make_shared<Literal>("foo"));
  list.Add(std::make_shared<Literal>("bar"));
  EXPECT_TRUE(list.Contains("foo"));
  EXPECT_TRUE(list.Contains("bar"));
  EXPECT_FALSE(list.Contains("baz"));
  auto refs = list.ToStrings();
  EXPECT_EQ(refs->size(), 2);
  EXPECT_EQ(refs->front(), intern_str("foo"));
  EXPECT_EQ(refs->back(), intern_str("bar"));
  EXPECT_EQ(list.Size(), 2);
  EXPECT_EQ(list.GetType(), ExpressionType::List);
  EXPECT_EQ(to_str(list), "List(Literal(foo),Literal(bar))");
}

TEST(Literal, Word) {
  Literal word(":foo");
  Literal not_word("b");
  EXPECT_TRUE(word.IsWord());
  EXPECT_EQ(word.GetWord(), "foo");
  EXPECT_FALSE(not_word.IsWord());
  EXPECT_EQ(not_word.GetWord(), "b");
}

TEST(Literal, Basic) {
  Literal literal("name");
  EXPECT_EQ(literal.AsString(), "name");
  EXPECT_TRUE(literal.Is("name"));

  EXPECT_EQ(to_str(literal), "Literal(name)");
  EXPECT_EQ(literal.GetType(), ExpressionType::Literal);
}