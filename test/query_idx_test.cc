#include "query_utils.h"
#include "../interpreter/query_index.h"
#include "../util/intern.h"
#include <gtest/gtest.h>
#include <utility>

using namespace atlas::interpreter;
using atlas::interpreter::query::and_q;
using atlas::interpreter::query::eq;
using atlas::meter::Tags;
using atlas::util::intern_str;

class QIdx : public QueryIndex<std::shared_ptr<Query>> {
 public:
  QIdx(const IndexMap& indexes, const EntryList& entries)
      : QueryIndex(indexes, entries) {}
  static Queries split(QueryPtr q) { return QueryIndex::split(std::move(q)); }
};

TEST(QueryIndex, Split) {
  auto q = and_q(eq("a", "1"), eq("b", "2"));
  auto v = std::vector<QIdx::QueryPtr>{q};

  auto split_qs = QIdx::split(q);
  EXPECT_EQ_QUERIES(v, split_qs);
}

TEST(QueryIndex, Empty) {
  QIdx::Queries queries;
  auto qi = QIdx::Build(queries);

  EXPECT_TRUE(qi->MatchingEntries(Tags{}).empty());
  EXPECT_TRUE(qi->MatchingEntries(Tags{{"a", "1"}}).empty());
}

TEST(QueryIndex, SingleQuerySimple) {
  QIdx::Queries queries;
  QueryPtr q1 = and_q(eq("a", "1"), eq("b", "2"));
  queries.emplace_back(q1);
  auto qi = QIdx::Build(queries);

  std::unordered_set<QueryPtr> expected_matches{q1};
  // not all tags are present
  EXPECT_TRUE(qi->MatchingEntries(Tags{}).empty());
  EXPECT_TRUE(qi->MatchingEntries(Tags{{"a", "1"}}).empty());

  // matches
  EXPECT_EQ_QUERIES(qi->MatchingEntries(Tags{{"a", "1"}, {"b", "2"}}),
                    expected_matches);
  EXPECT_EQ_QUERIES(
      qi->MatchingEntries(Tags{{"a", "1"}, {"b", "2"}, {"c", "3"}}),
      expected_matches);

  // a doesn't match
  EXPECT_TRUE(
      qi->MatchingEntries(Tags{{"a", "2"}, {"b", "2"}, {"c", "3"}}).empty());

  // b doesn't match
  EXPECT_TRUE(
      qi->MatchingEntries(Tags{{"a", "1"}, {"b", "3"}, {"c", "3"}}).empty());
}

StringRefs genStrRefs(size_t n) {
  StringRefs res;
  res.reserve(n);
  for (auto i = 0u; i < n; ++i) {
    res.emplace_back(intern_str(std::to_string(i).c_str()));
  }
  return res;
}

TEST(QueryIndex, InExpansionIsLimited) {
  QueryPtr q1 = query::in("a", genStrRefs(10000));
  QueryPtr q2 = query::in("b", genStrRefs(10000));
  QueryPtr q3 = query::in("c", genStrRefs(10000));
  QueryPtr q = and_q(and_q(q1, q2), q3);
  Queries qs{q};
  auto qi = QIdx::Build(qs);
  EXPECT_TRUE(qi->Matches(Tags{{"a", "1"}, {"b", "9999"}, {"c", "727"}}));
  EXPECT_FALSE(qi->Matches(Tags{{"a", "1"}, {"b", "10000"}, {"c", "727"}}));
}

TEST(QueryIndex, ComplexSingleQuery) {
  QueryPtr q = and_q(and_q(eq("a", "1"), eq("b", "2")), query::has("c"));
  auto qi = QIdx::Build({Queries{q}});

  EXPECT_FALSE(qi->Matches(Tags{}));
  EXPECT_FALSE(qi->Matches(Tags{{"a", "1"}}));
  EXPECT_FALSE(qi->Matches(Tags{{"a", "1"}, {"b", "2"}}));
  EXPECT_TRUE(qi->Matches(Tags{{"a", "1"}, {"b", "2"}, {"c", "3"}}));
  EXPECT_FALSE(qi->Matches(Tags{{"a", "2"}, {"b", "2"}, {"c", "3"}}));
  EXPECT_FALSE(qi->Matches(Tags{{"a", "1"}, {"b", "1"}, {"c", "3"}}));
}

TEST(QueryIndex, ManyQueries) {
  QueryPtr cpu_usage = eq("name", "cpuUsage");
  Queries disk_usage_per_node;
  disk_usage_per_node.reserve(100);
  QueryPtr disk_usage = eq("name", "diskUsage");
  for (auto i = 0; i < 100; ++i) {
    auto node = fmt::format("i-{:05}", i);
    disk_usage_per_node.emplace_back(
        and_q(disk_usage, eq("nf.node", node.c_str())));
  }
  Queries queries;
  queries.reserve(102);
  queries.push_back(cpu_usage);
  queries.push_back(disk_usage);
  append_to_vector(&queries, std::move(disk_usage_per_node));
  auto qi = QIdx::Build(queries);

  EXPECT_FALSE(qi->Matches(Tags{}));
  EXPECT_FALSE(qi->Matches(Tags{{"a", "1"}}));

  std::unordered_set<QueryPtr> cpu_usage_set{cpu_usage};
  EXPECT_EQ_QUERIES(
      qi->MatchingEntries(Tags{{"name", "cpuUsage"}, {"nf.node", "unknown"}}),
      cpu_usage_set);
  EXPECT_EQ_QUERIES(
      qi->MatchingEntries(Tags{{"name", "cpuUsage"}, {"nf.node", "i-00099"}}),
      cpu_usage_set);

  std::unordered_set<QueryPtr> expected_disk1{
      disk_usage, and_q(disk_usage, eq("nf.node", "i-00099"))};
  EXPECT_EQ_QUERIES(
      qi->MatchingEntries(Tags{{"name", "diskUsage"}, {"nf.node", "i-00099"}}),
      expected_disk1);

  std::unordered_set<QueryPtr> expected_disk2{disk_usage};
  EXPECT_EQ_QUERIES(
      qi->MatchingEntries(Tags{{"name", "diskUsage"}, {"nf.node", "unknown"}}),
      expected_disk2);

  EXPECT_FALSE(qi->Matches(Tags{{"nf.node", "unknown"}}));
}