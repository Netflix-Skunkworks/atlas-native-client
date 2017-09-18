#include "../interpreter/query.h"
#include <gtest/gtest.h>

using namespace atlas::meter;
using namespace atlas::interpreter;
using atlas::util::intern_str;

Tags tags{{"k", "bar"}, {"name", "foo"}};
Tags tags2{{"name", "foo"}, {"k1", "ba"}};

TEST(Queries, HasKey) {
  const Query& name_query = HasKeyQuery("name");
  EXPECT_TRUE(name_query.Matches(tags));

  const Query& key1 = HasKeyQuery("k");
  EXPECT_TRUE(key1.Matches(tags));

  const Query& key2 = HasKeyQuery("k2");
  EXPECT_FALSE(key2.Matches(tags));
}

TEST(Queries, RegexCaret) {
  const Query& q1 = RegexQuery("name", "^f", false);
  EXPECT_TRUE(q1.Matches(tags));

  const Query& q1_case = RegexQuery("name", "^F", false);
  EXPECT_FALSE(q1_case.Matches(tags));

  const Query& q2 = RegexQuery("k", "^f", false);
  EXPECT_FALSE(q2.Matches(tags));

  const Query& q3 = RegexQuery("k2", "^f", false);
  EXPECT_FALSE(q3.Matches(tags));
}

TEST(Queries, RegexNoCaret) {
  const Query& q1 = RegexQuery("name", "f", false);
  EXPECT_TRUE(q1.Matches(tags));

  const Query& q1_case = RegexQuery("name", "F", false);
  EXPECT_FALSE(q1_case.Matches(tags));

  const Query& q2 = RegexQuery("k", "f", false);
  EXPECT_FALSE(q2.Matches(tags));

  const Query& q3 = RegexQuery("k2", "f", false);
  EXPECT_FALSE(q3.Matches(tags));
}

TEST(Queries, RegexIgnoreCase) {
  const Query& q1 = RegexQuery("name", "fo", true);
  EXPECT_TRUE(q1.Matches(tags));

  const Query& q1_case = RegexQuery("name", "FO", true);
  EXPECT_TRUE(q1_case.Matches(tags));

  const Query& q2 = RegexQuery("k", "B", false);
  EXPECT_FALSE(q2.Matches(tags));

  const Query& q3 = RegexQuery("k", "B", true);
  EXPECT_TRUE(q3.Matches(tags));
}

TEST(Queries, RelopEq) {
  auto eq1 = query::eq("name", "foo");
  EXPECT_TRUE(eq1->Matches(tags));

  auto eq2 = query::eq("name", "foobar");
  EXPECT_FALSE(eq2->Matches(tags));

  auto eq3 = query::eq("k", "bar");
  EXPECT_TRUE(eq3->Matches(tags));

  auto eq4 = query::eq("k1", "bar");
  EXPECT_FALSE(eq4->Matches(tags));
}

TEST(Queries, RelopLE) {
  auto q1 = query::le("name", "foo");
  EXPECT_TRUE(q1->Matches(tags));

  auto q2 = query::le("name", "z");
  EXPECT_TRUE(q2->Matches(tags));

  auto longer = query::le("k", "ba");
  EXPECT_FALSE(longer->Matches(tags));

  auto shorter = query::le("k", "barr");
  EXPECT_TRUE(shorter->Matches(tags));

  auto q4 = query::le("k1", "bar");
  EXPECT_FALSE(q4->Matches(tags));

  auto q5 = query::le("k", "c");
  EXPECT_TRUE(q5->Matches(tags));
}

TEST(Queries, RelopLT) {
  auto q1 = query::lt("name", "foo");
  EXPECT_FALSE(q1->Matches(tags));

  auto q2 = query::lt("name", "z");
  EXPECT_TRUE(q2->Matches(tags));

  auto longer = query::lt("k", "ba");
  EXPECT_FALSE(longer->Matches(tags));

  auto shorter = query::lt("k", "barr");
  EXPECT_TRUE(shorter->Matches(tags));

  auto q4 = query::lt("k1", "bar");
  EXPECT_FALSE(q4->Matches(tags));

  auto q5 = query::lt("k", "c");
  EXPECT_TRUE(q5->Matches(tags));
}

TEST(Queries, RelopGE) {
  auto q1 = query::ge("name", "foo");
  EXPECT_TRUE(q1->Matches(tags));

  auto q2 = query::ge("name", "z");
  EXPECT_FALSE(q2->Matches(tags));

  auto longer = query::ge("k", "ba");
  EXPECT_TRUE(longer->Matches(tags));

  auto shorter = query::ge("k", "barr");
  EXPECT_FALSE(shorter->Matches(tags));

  auto q4 = query::ge("k1", "bar");
  EXPECT_FALSE(q4->Matches(tags));

  auto q5 = query::ge("k", "c");
  EXPECT_FALSE(q5->Matches(tags));
}

TEST(Queries, RelopGT) {
  auto q1 = query::gt("name", "foo");
  EXPECT_FALSE(q1->Matches(tags));

  auto q2 = query::gt("name", "z");
  EXPECT_FALSE(q2->Matches(tags));

  auto longer = query::gt("k", "ba");
  EXPECT_TRUE(longer->Matches(tags));

  auto shorter = query::gt("k", "barr");
  EXPECT_FALSE(shorter->Matches(tags));

  auto q4 = query::gt("k1", "bar");
  EXPECT_FALSE(q4->Matches(tags));

  auto q5 = query::gt("k", "c");
  EXPECT_FALSE(q5->Matches(tags));
}

TEST(Queries, In) {
  auto in1 =
      query::in("k", {intern_str("foo"), intern_str("bar"), intern_str("baz")});
  EXPECT_TRUE(in1->Matches(tags));

  EXPECT_FALSE(in1->Matches(tags2));
}

TEST(Queries, True) {
  auto q = query::true_q();
  EXPECT_TRUE(q->Matches(tags));
}

TEST(Queries, False) {
  auto q = query::false_q();
  EXPECT_FALSE(q->Matches(tags));
}

TEST(Queries, Not) {
  auto not_false = query::not_q(query::false_q());

  EXPECT_TRUE(not_false->Matches(tags));
  auto not_true = query::not_q(query::true_q());
  EXPECT_FALSE(not_true->Matches(tags));

  auto not_eq_q = query::not_q(query::eq("name", "foo"));
  EXPECT_FALSE(not_eq_q->Matches(tags));
}

TEST(Queries, And) {
  auto and1 = query::and_q(query::false_q(), query::true_q());
  EXPECT_FALSE(and1->Matches(tags));
  EXPECT_TRUE(and1->IsFalse());

  auto and2 = query::and_q(query::true_q(), query::false_q());
  EXPECT_FALSE(and2->Matches(tags));
  EXPECT_TRUE(and2->IsFalse());

  auto and3 = query::and_q(query::reic("name", "FO"), query::eq("k", "bar"));
  EXPECT_TRUE(and3->Matches(tags));

  auto and4 = query::and_q(query::re("name", "FO"), query::eq("k", "sps"));
  EXPECT_FALSE(and4->Matches(tags));
}

TEST(Queries, Or) {
  auto or1 = query::or_q(query::false_q(), query::true_q());
  EXPECT_TRUE(or1->Matches(tags));
  EXPECT_TRUE(or1->IsTrue());

  auto or2 = query::or_q(query::true_q(), query::false_q());
  EXPECT_TRUE(or2->Matches(tags));
  EXPECT_TRUE(or2->IsTrue());

  auto or3 = query::or_q(query::reic("name", "FO"), query::eq("k", "baz"));
  EXPECT_TRUE(or3->Matches(tags));

  auto or4 = query::or_q(query::re("name", "FO"), query::eq("k", "bar"));
  EXPECT_TRUE(or4->Matches(tags));
}
