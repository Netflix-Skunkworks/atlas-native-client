#include "../interpreter/query.h"
#include <gtest/gtest.h>

using namespace atlas::meter;
using namespace atlas::interpreter;

TEST(Queries, HasKey) {
  Tags tags{{"k", "v"}, {"name", "foo"}};

  const Query& name_query = HasKeyQuery("name");
  EXPECT_TRUE(name_query.Matches(tags));

  const Query& key1 = HasKeyQuery("k");
  EXPECT_TRUE(key1.Matches(tags));

  const Query& key2 = HasKeyQuery("k2");
  EXPECT_FALSE(key2.Matches(tags));
}

TEST(Queries, RegexCaret) {
  Tags tags{{"name", "foo"}, {"k", "v"}};

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
  Tags tags{{"name", "foo"}, {"k", "v"}};

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
  Tags tags{{"name", "foo"}, {"k", "v"}};

  const Query& q1 = RegexQuery("name", "fo", true);
  EXPECT_TRUE(q1.Matches(tags));

  const Query& q1_case = RegexQuery("name", "FO", true);
  EXPECT_TRUE(q1_case.Matches(tags));

  const Query& q2 = RegexQuery("k", "V", false);
  EXPECT_FALSE(q2.Matches(tags));

  const Query& q3 = RegexQuery("k", "V", true);
  EXPECT_TRUE(q3.Matches(tags));
}

TEST(Queries, RelopEq) {
  Tags tags{{"name", "foo"}, {"k1", "bar"}};

  auto eq1 = query::eq("name", "foo");
  EXPECT_TRUE(eq1->Matches(tags));

  auto eq2 = query::eq("name", "foobar");
  EXPECT_FALSE(eq2->Matches(tags));

  auto eq3 = query::eq("k1", "bar");
  EXPECT_TRUE(eq3->Matches(tags));

  auto eq4 = query::eq("k", "bar");
  EXPECT_FALSE(eq4->Matches(tags));
}

TEST(Queries, RelopLE) {
  Tags tags{{"name", "foo"}, {"k1", "bar"}};

  auto q1 = query::le("name", "foo");
  EXPECT_TRUE(q1->Matches(tags));

  auto q2 = query::le("name", "z");
  EXPECT_TRUE(q2->Matches(tags));

  auto longer = query::le("k1", "ba");
  EXPECT_FALSE(longer->Matches(tags));

  auto shorter = query::le("k1", "barr");
  EXPECT_TRUE(shorter->Matches(tags));

  auto q4 = query::le("k", "bar");
  EXPECT_FALSE(q4->Matches(tags));

  auto q5 = query::le("k1", "c");
  EXPECT_TRUE(q5->Matches(tags));
}

TEST(Queries, RelopLT) {
  Tags tags{{"name", "foo"}, {"k1", "bar"}};

  auto q1 = query::lt("name", "foo");
  EXPECT_FALSE(q1->Matches(tags));

  auto q2 = query::lt("name", "z");
  EXPECT_TRUE(q2->Matches(tags));

  auto longer = query::lt("k1", "ba");
  EXPECT_FALSE(longer->Matches(tags));

  auto shorter = query::lt("k1", "barr");
  EXPECT_TRUE(shorter->Matches(tags));

  auto q4 = query::lt("k", "bar");
  EXPECT_FALSE(q4->Matches(tags));

  auto q5 = query::lt("k1", "c");
  EXPECT_TRUE(q5->Matches(tags));
}

TEST(Queries, RelopGE) {
  Tags tags{{"name", "foo"}, {"k1", "bar"}};

  auto q1 = query::ge("name", "foo");
  EXPECT_TRUE(q1->Matches(tags));

  auto q2 = query::ge("name", "z");
  EXPECT_FALSE(q2->Matches(tags));

  auto longer = query::ge("k1", "ba");
  EXPECT_TRUE(longer->Matches(tags));

  auto shorter = query::ge("k1", "barr");
  EXPECT_FALSE(shorter->Matches(tags));

  auto q4 = query::ge("k", "bar");
  EXPECT_FALSE(q4->Matches(tags));

  auto q5 = query::ge("k1", "c");
  EXPECT_FALSE(q5->Matches(tags));
}

TEST(Queries, RelopGT) {
  Tags tags{{"name", "foo"}, {"k1", "bar"}};

  auto q1 = query::gt("name", "foo");
  EXPECT_FALSE(q1->Matches(tags));

  auto q2 = query::gt("name", "z");
  EXPECT_FALSE(q2->Matches(tags));

  auto longer = query::gt("k1", "ba");
  EXPECT_TRUE(longer->Matches(tags));

  auto shorter = query::gt("k1", "barr");
  EXPECT_FALSE(shorter->Matches(tags));

  auto q4 = query::gt("k", "bar");
  EXPECT_FALSE(q4->Matches(tags));

  auto q5 = query::gt("k1", "c");
  EXPECT_FALSE(q5->Matches(tags));
}

TEST(Queries, In) {
  Tags tags{{"name", "foo"}, {"k1", "bar"}};
  auto in1 = query::in("k1", {"foo", "bar", "baz"});
  EXPECT_TRUE(in1->Matches(tags));

  Tags tags2{{"name", "foo"}, {"k1", "ba"}};
  EXPECT_FALSE(in1->Matches(tags2));
}

TEST(Queries, True) {
  Tags tags{{"name", "foo"}, {"k1", "bar"}};
  auto q = query::true_q();
  EXPECT_TRUE(q->Matches(tags));
}

TEST(Queries, False) {
  Tags tags{{"name", "foo"}, {"k1", "bar"}};
  auto q = query::false_q();
  EXPECT_FALSE(q->Matches(tags));
}

TEST(Queries, Not) {
  Tags tags{{"name", "foo"}, {"k1", "bar"}};
  auto not_false = query::not_q(query::false_q());

  EXPECT_TRUE(not_false->Matches(tags));
  auto not_true = query::not_q(query::true_q());
  EXPECT_FALSE(not_true->Matches(tags));

  auto not_eq_q = query::not_q(query::eq("name", "foo"));
  EXPECT_FALSE(not_eq_q->Matches(tags));
}

TEST(Queries, And) {
  Tags tags{{"name", "foo"}, {"k1", "bar"}};

  auto and1 = query::and_q(query::false_q(), query::true_q());
  EXPECT_FALSE(and1->Matches(tags));
  EXPECT_TRUE(and1->IsFalse());

  auto and2 = query::and_q(query::true_q(), query::false_q());
  EXPECT_FALSE(and2->Matches(tags));
  EXPECT_TRUE(and2->IsFalse());

  auto and3 = query::and_q(query::reic("name", "FO"), query::eq("k1", "bar"));
  EXPECT_TRUE(and3->Matches(tags));

  auto and4 = query::and_q(query::re("name", "FO"), query::eq("k1", "sps"));
  EXPECT_FALSE(and4->Matches(tags));
}

TEST(Queries, Or) {
  Tags tags{{"name", "foo"}, {"k1", "bar"}};

  auto or1 = query::or_q(query::false_q(), query::true_q());
  EXPECT_TRUE(or1->Matches(tags));
  EXPECT_TRUE(or1->IsTrue());

  auto or2 = query::or_q(query::true_q(), query::false_q());
  EXPECT_TRUE(or2->Matches(tags));
  EXPECT_TRUE(or2->IsTrue());

  auto or3 = query::or_q(query::reic("name", "FO"), query::eq("k1", "baz"));
  EXPECT_TRUE(or3->Matches(tags));

  auto or4 = query::or_q(query::re("name", "FO"), query::eq("k1", "bar"));
  EXPECT_TRUE(or4->Matches(tags));
}
