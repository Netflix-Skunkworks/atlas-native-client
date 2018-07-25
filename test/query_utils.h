#pragma once

#include <gtest/gtest.h>
#include <vector>
#include <unordered_set>
#include "../interpreter/query.h"

using atlas::interpreter::Query;
using atlas::interpreter::QueryType;
using QueryPtr = std::shared_ptr<Query>;
using Queries = std::vector<QueryPtr>;

inline std::string queries_to_str(const Queries& qs) {
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

inline void expect_eq_queries(Queries& lhs, Queries& rhs) {
  if (lhs.size() != rhs.size()) {
    FAIL() << "lhs.size()=" << lhs.size() << " != rhs.size()=" << rhs.size()
           << "\n"
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
