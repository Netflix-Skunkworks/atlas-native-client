#include "query_utils.h"
#include "test_utils.h"

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

  EXPECT_EQ("RelopQuery(k>c)", to_str(*q5));
}

TEST(Queries, In) {
  auto in1 =
      query::in("k", {intern_str("foo"), intern_str("bar"), intern_str("baz")});
  EXPECT_TRUE(in1->Matches(tags));

  EXPECT_FALSE(in1->Matches(tags2));
  EXPECT_EQ("InQuery(k,[foo,bar,baz])", to_str(*in1));
}

TEST(Queries, True) {
  auto q = query::true_q();
  EXPECT_TRUE(q->Matches(tags));
  EXPECT_EQ(to_str(*q), "TrueQuery");
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

  EXPECT_EQ("TrueQuery", to_str(*not_false));
  EXPECT_EQ("FalseQuery", to_str(*not_true));
  EXPECT_EQ("NotQuery(RelopQuery(name=foo))", to_str(*not_eq_q));
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

  auto and4 = query::and_q(query::re("name", "FO"), query::le("k", "sps"));
  EXPECT_FALSE(and4->Matches(tags));

  EXPECT_EQ(
      "AndQuery(RelopQuery(k<="
      "sps), RegexQuery(name ~ FO))",
      to_str(*and4));
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

  auto or4 = query::or_q(query::re("name", "FO"), query::ge("k", "bar"));
  EXPECT_TRUE(or4->Matches(tags));

  EXPECT_EQ("OrQuery(RelopQuery(k>=bar), RegexQuery(name ~ FO))", to_str(*or4));
}

TEST(Queries, Equals) {
  EXPECT_TRUE(query::eq("foo", "bar")->Equals(*query::eq("foo", "bar")));
  EXPECT_FALSE(query::eq("foo", "bar")->Equals(*query::eq("foo", "bar1")));
  EXPECT_TRUE(query::re("foo", "bar")->Equals(*query::re("foo", "bar")));
  EXPECT_FALSE(query::re("foo", "bar")->Equals(*query::re("foo", "bar1")));
  EXPECT_FALSE(query::re("foo", "bar")->Equals(*query::reic("foo", "bar")));
  EXPECT_TRUE(query::reic("foo", "bar")->Equals(*query::reic("foo", "bar")));
  EXPECT_FALSE(query::reic("foo", "bar")->Equals(*query::reic("foo", "barx")));
  EXPECT_FALSE(query::reic("foo", "bar")->Equals(*query::re("foo", "bar")));
  EXPECT_TRUE(query::gt("foo", "bar")->Equals(*query::gt("foo", "bar")));
  EXPECT_FALSE(query::gt("foo", "bar")->Equals(*query::gt("foox", "bar")));
  EXPECT_FALSE(query::gt("foo", "bar")->Equals(*query::ge("foo", "bar")));
  EXPECT_TRUE(query::lt("foo", "bar")->Equals(*query::lt("foo", "bar")));
  EXPECT_FALSE(query::lt("foo", "bar")->Equals(*query::lt("foox", "bar")));
  EXPECT_FALSE(query::lt("foo", "bar")->Equals(*query::le("foo", "bar")));

  EXPECT_TRUE(
      query::and_q(query::eq("a", "b"), query::re("c", "d"))
          ->Equals(*query::and_q(query::eq("a", "b"), query::re("c", "d"))));
  EXPECT_TRUE(
      query::or_q(query::eq("a", "b"), query::re("c", "d"))
          ->Equals(*query::or_q(query::eq("a", "b"), query::re("c", "d"))));
  EXPECT_FALSE(
      query::and_q(query::eq("a", "b"), query::re("c", "d"))
          ->Equals(*query::and_q(query::eq("a", "b"), query::reic("c", "d"))));
  EXPECT_FALSE(
      query::or_q(query::eq("a", "b"), query::re("c", "d"))
          ->Equals(*query::or_q(query::eq("a", "b"), query::reic("c", "d"))));

  EXPECT_TRUE(query::not_q(query::eq("a", "b"))
                  ->Equals(*query::not_q(query::eq("a", "b"))));
  EXPECT_FALSE(query::not_q(query::eq("a", "b"))
                   ->Equals(*query::not_q(query::lt("a", "b"))));

  auto lst1 = StringRefs{intern_str("a"), intern_str("b")};
  auto lst2 = StringRefs{intern_str("a"), intern_str("b")};
  EXPECT_TRUE(query::in("foo", lst1)->Equals(*query::in("foo", lst2)));

  lst2[0] = intern_str("aa");
  EXPECT_FALSE(query::in("foo", lst1)->Equals(*query::in("foo", lst2)));

  EXPECT_TRUE(HasKeyQuery("foo").Equals(HasKeyQuery("foo")));
  EXPECT_FALSE(HasKeyQuery("foo").Equals(HasKeyQuery("bar")));

  EXPECT_TRUE(TrueQuery{}.Equals(TrueQuery{}));
  EXPECT_FALSE(TrueQuery{}.Equals(HasKeyQuery{"foo"}));
  EXPECT_TRUE(FalseQuery{}.Equals(FalseQuery{}));
  EXPECT_FALSE(FalseQuery{}.Equals(TrueQuery{}));
}

TEST(Queries, InvalidRegex) {
  auto q = RegexQuery("name", "*foo*", false);
  Tags t{{"k", "v"}};
  // make sure it doesn't throw
  EXPECT_FALSE(q.Matches(t));

  EXPECT_EQ("RegexQuery(name ~ *foo*)", to_str(q));
}

TEST(Queries, DnfSingleQuery) {
  auto q = std::make_shared<HasKeyQuery>("a");
  auto dnf = query::dnf_list(q);
  auto expected = Queries{q};

  expect_eq_queries(dnf, expected);
}

TEST(Queries, DnfAnd) {
  auto q = query::and_q(query::eq("a", "1"), query::eq("b", "2"));
  auto dnf = query::dnf_list(q);
  auto expected = Queries{q};

  expect_eq_queries(dnf, expected);
}

TEST(Queries, DnfOrAnd) {
  using query::eq;

  QueryPtr a = eq("a", "1");
  QueryPtr b = eq("b", "2");
  QueryPtr c = eq("c", "3");
  QueryPtr or_query = query::or_q(a, b);
  QueryPtr q = query::and_q(or_query, query::eq("c", "3"));

  auto dnf = query::dnf_list(q);
  QueryPtr ac = query::and_q(a, c);
  QueryPtr bc = query::and_q(b, c);
  auto expected = Queries{ac, bc};
  expect_eq_queries(dnf, expected);
}

TEST(Queries, DnfOrAndOr) {
  using query::eq;

  QueryPtr a = eq("a", "1");
  QueryPtr b = eq("b", "2");
  QueryPtr c = eq("c", "3");
  QueryPtr d = eq("d", "4");
  QueryPtr or2 = query::or_q(a, b);
  QueryPtr or1 = query::or_q(c, d);
  QueryPtr q = query::and_q(or1, or2);

  auto dnf = query::dnf_list(q);
  QueryPtr ac = query::and_q(a, c);
  QueryPtr ad = query::and_q(a, d);
  QueryPtr bc = query::and_q(b, c);
  QueryPtr bd = query::and_q(b, d);
  auto expected = Queries{ac, ad, bc, bd};
  expect_eq_queries(dnf, expected);
}

TEST(Queries, DnfNotOr) {
  using query::eq;

  QueryPtr a = eq("a", "1");
  QueryPtr b = eq("b", "2");
  QueryPtr or1 = query::or_q(a, b);
  QueryPtr q = query::not_q(or1);

  auto dnf = query::dnf_list(q);
  QueryPtr nota = query::not_q(a);
  QueryPtr notb = query::not_q(b);
  QueryPtr andq = query::and_q(notb, nota);
  auto expected = Queries{andq};
  expect_eq_queries(dnf, expected);
}

TEST(Queries, DnfNotAnd) {
  using query::eq;

  QueryPtr a = eq("a", "1");
  QueryPtr b = eq("b", "2");
  QueryPtr and1 = query::and_q(a, b);
  QueryPtr q = query::not_q(and1);

  auto dnf = query::dnf_list(q);
  QueryPtr nota = query::not_q(a);
  QueryPtr notb = query::not_q(b);
  auto expected = Queries{nota, notb};
  expect_eq_queries(dnf, expected);
}
