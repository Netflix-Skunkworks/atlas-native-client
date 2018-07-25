#pragma once

#include <gtest/gtest.h>
#include <vector>
#include <unordered_set>
#include "../interpreter/query.h"

using atlas::interpreter::Query;
using atlas::interpreter::QueryType;
using QueryPtr = std::shared_ptr<Query>;
using Queries = std::vector<QueryPtr>;

template <typename T>
inline std::string queries_to_str(const T& qs) {
  std::ostringstream os;
  os << '[';
  auto first = true;
  for (const auto& q : qs) {
    if (first) {
      first = false;
    } else {
      os << ',';
    }
    q->Dump(os);
  }
  os << ']';
  return os.str();
}

inline std::string query_to_str(const Query& q) {
  std::ostringstream os;
  q.Dump(os);
  return os.str();
}

inline void expect_eq_queries(const char* file, int line, const Queries& lhs,
                              const Queries& rhs) {
  if (lhs.size() != rhs.size()) {
    FAIL() << file << ":" << line << ": lhs.size()=" << lhs.size()
           << " != rhs.size()=" << rhs.size() << "\n"
           << queries_to_str(lhs) << " != " << queries_to_str(rhs);
  }

  auto i = 0u;
  for (const auto& q1 : lhs) {
    auto& q2 = rhs[i++];
    if (!q1->Equals(*q2)) {
      FAIL() << query_to_str(*q1) << " != " << query_to_str(*q2) << "\n"
             << queries_to_str(lhs) << " !=\n"
             << queries_to_str(rhs);
    }
  }
}

inline void expect_eq_queries(const char* file, int line,
                              const std::unordered_set<QueryPtr>& lhs,
                              const std::unordered_set<QueryPtr>& rhs) {
  if (lhs.size() != rhs.size()) {
    std::cerr << fmt::format("{}:{} lhs.size()={} != rhs.size()={}\n", file,
                             line, lhs.size(), rhs.size());
    GTEST_FAIL();
  }

  for (const auto& q1 : lhs) {
    auto found = rhs.find(q1) != rhs.end();
    if (!found) {
      std::cerr << fmt::format("{}:{} {} not found in {}\n", file, line,
                               query_to_str(*q1), queries_to_str(rhs));
      GTEST_FAIL();
    }
  }
}

#define EXPECT_EQ_QUERIES(a, b) expect_eq_queries(__FILE__, __LINE__, a, b)
