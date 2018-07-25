#pragma once

#include <ska/flat_hash_map.hpp>
#include <unordered_set>
#include <ostream>
#include <sstream>
#include <iostream>
#include <utility>
#include "query.h"
#include "../util/collections.h"
#include "../util/logger.h"
namespace atlas {
namespace interpreter {

template <typename T>
struct QueryIndexEntry {
  std::shared_ptr<Query> query;
  T value;
};

template <typename T>
class QueryIndexAnnotated {
 public:
  using Filters = ska::flat_hash_set<std::shared_ptr<RelopQuery>>;
  QueryIndexAnnotated(QueryIndexEntry<T> e, Filters f) noexcept
      : entry{std::move(e)}, filters{std::move(f)} {}

  QueryIndexEntry<T> entry;
  Filters filters;

  std::vector<std::pair<std::shared_ptr<RelopQuery>, QueryIndexAnnotated<T>>>
  to_query_annotated_list() const {
    std::vector<std::pair<std::shared_ptr<RelopQuery>, QueryIndexAnnotated<T>>>
        res;
    res.reserve(filters.size());
    for (const auto& filter : filters) {
      Filters new_filters{filters};
      new_filters.erase(filter);
      res.emplace_back(std::make_pair(
          filter, QueryIndexAnnotated{entry, std::move(new_filters)}));
    }
    return res;
  }

  std::string to_string() const {
    std::ostringstream os;
    os << "(entry.query=";
    entry.query->Dump(os);
    os << ", filters=[";
    auto first = true;
    for (const auto& f : filters) {
      if (first) {
        first = false;
      } else {
        os << ',';
      }
      f->Dump(os);
    }
    os << "])";
    return os.str();
  }
};

}  // namespace interpreter
}  // namespace atlas

namespace std {
template <typename T>
struct hash<atlas::interpreter::QueryIndexEntry<T>> {
  size_t operator()(const atlas::interpreter::QueryIndexEntry<T>& a) const {
    return a.query->Hash() ^ hash<T>()(a.value);
  }
};

template <typename T>
struct equal_to<atlas::interpreter::QueryIndexEntry<T>> {
  bool operator()(const atlas::interpreter::QueryIndexEntry<T>& lhs,
                  const atlas::interpreter::QueryIndexEntry<T>& rhs) const {
    return lhs.query->Equals(*rhs.query) && equal_to<T>()(lhs.value, rhs.value);
  }
};

template <typename T>
struct hash<atlas::interpreter::QueryIndexAnnotated<T>> {
  size_t operator()(const atlas::interpreter::QueryIndexAnnotated<T>& a) const {
    auto res = hash<atlas::interpreter::QueryIndexEntry<T>>()(a.entry);
    for (const auto& f : a.filters) {
      res <<= 1;
      res ^= f->Hash();
    }
    return res;
  }
};

template <typename T>
struct equal_to<atlas::interpreter::QueryIndexAnnotated<T>> {
  bool operator()(const atlas::interpreter::QueryIndexAnnotated<T>& lhs,
                  const atlas::interpreter::QueryIndexAnnotated<T>& rhs) const {
    bool eq_entries = equal_to<atlas::interpreter::QueryIndexEntry<T>>()(
        lhs.entry, rhs.entry);
    if (!eq_entries) return false;

    if (lhs.filters.size() != rhs.filters.size()) {
      return false;
    }

    for (const auto& f : lhs.filters) {
      auto found = rhs.filters.find(f) != rhs.filters.end();
      if (!found) return false;
    }

    return true;
  }
};

template <typename T>
struct hash<vector<atlas::interpreter::QueryIndexAnnotated<T>>> {
  size_t operator()(
      const vector<atlas::interpreter::QueryIndexAnnotated<T>>& v) const {
    auto res = static_cast<size_t>(0);
    for (const auto& e : v) {
      res = (res << 4u) ^
            std::hash<atlas::interpreter::QueryIndexAnnotated<T>>()(e);
    }
    return res;
  }
};

template <typename T>
struct equal_to<vector<atlas::interpreter::QueryIndexAnnotated<T>>> {
  bool operator()(
      const vector<atlas::interpreter::QueryIndexAnnotated<T>>& lhs,
      const vector<atlas::interpreter::QueryIndexAnnotated<T>>& rhs) const {
    if (lhs.size() != rhs.size()) return false;
    for (auto i = 0u; i < lhs.size(); ++i) {
      if (!equal_to<atlas::interpreter::QueryIndexAnnotated<T>>()(lhs[i],
                                                                  rhs[i]))
        return false;
    }
    return true;
  }
};
}  // namespace std

namespace atlas {
namespace interpreter {
template <typename T>
class QueryIndex {
 public:
  using QueryPtr = std::shared_ptr<Query>;
  using RelopQueryPtr = std::shared_ptr<RelopQuery>;
  using Queries = std::vector<QueryPtr>;
  using RelopQueries = std::vector<RelopQueryPtr>;
  using IndexMap =
      ska::flat_hash_map<RelopQueryPtr, std::shared_ptr<QueryIndex<T>>>;
  using EntryList = std::vector<QueryIndexEntry<T>>;
  using AnnotatedList = std::vector<QueryIndexAnnotated<T>>;
  using AnnotatedIdxMap =
      ska::flat_hash_map<AnnotatedList, std::shared_ptr<QueryIndex<T>>>;

  QueryIndex(IndexMap indexes, EntryList entries)
      : indexes_(std::move(indexes)), entries_(std::move(entries)) {}

  static std::shared_ptr<QueryIndex<T>> Create(const EntryList& entries) {
    std::vector<QueryIndexAnnotated<T>> annotated;
    annotated.reserve(entries.size() * 2);
    for (const auto& entry : entries) {
      Queries to_split = query::dnf_list(entry.query);
      for (auto& raw_q : to_split) {
        Queries lst = split(raw_q);
        for (auto& q : lst) {
          annotated.emplace_back(annotate(QueryIndexEntry<T>{q, entry.value}));
        }
      }
    }
    AnnotatedIdxMap idx_map;
    return create_impl(&idx_map, annotated);
  }

  static std::shared_ptr<QueryIndex<QueryPtr>> Build(const Queries& queries) {
    EntryList entries;
    entries.reserve(queries.size());
    std::transform(queries.begin(), queries.end(), std::back_inserter(entries),
                   [](const QueryPtr& q) {
                     return QueryIndexEntry<QueryPtr>{q, q};
                   });
    return QueryIndex<QueryPtr>::Create(entries);
  }

  bool Matches(const meter::Tags& tags) const noexcept {
    return !MatchingEntries(tags).empty();
  }

  std::unordered_set<T> MatchingEntries(const meter::Tags& tags) const
      noexcept {
    RelopQueries queries;
    std::transform(tags.begin(), tags.end(), std::back_inserter(queries),
                   [](const std::pair<util::StrRef, util::StrRef>& tag) {
                     return std::make_shared<RelopQuery>(tag.first, tag.second,
                                                         RelOp::EQ);
                   });
    return matching_entries(tags, queries.begin(), queries.end());
  }

  std::string ToString() const noexcept {
    std::string res;
    gen_to_string(&res, 0);
    return res;
  }

 protected:
  IndexMap indexes_;
  EntryList entries_;

  static std::string query_to_str(const Query& query) {
    std::ostringstream os;
    query.Dump(os);
    return os.str();
  }

  void gen_to_string(std::string* res, size_t indent) const {
    std::string pad1(indent, ' ');
    std::string pad2(indent + 1, ' ');

    std::string& buf = *res;
    buf += pad1;
    buf += "children\n";
    for (const auto& kv : indexes_) {
      buf += pad2;
      buf += query_to_str(*kv.first);
      buf += '\n';
      kv.second->gen_to_string(res, indent + 2);
    }
    buf += pad1;
    buf += "queries\n";
    for (const auto& e : entries_) {
      buf += pad2;
      buf += query_to_str(*e.query);
      buf += '\n';
    }
  }

  static std::string dump_map(AnnotatedIdxMap* map) {
    std::ostringstream os;
    os << "(sz=" << map->size() << ',';
    for (const auto& kv : *map) {
      os << dump_anno_entries(kv.first) << ',';
    }
    os << ')';
    return os.str();
  }

  template <class C>
  static std::string dump_set(const C& set) {
    std::ostringstream os;
    bool first = true;
    for (const auto& q : set) {
      if (first) {
        first = false;
      } else {
        os << ',';
      }
      q->Dump(os);
    }
    return os.str();
  }

  static std::string dump_anno(const QueryIndexAnnotated<T>& anno) {
    std::ostringstream os;
    os << "anno(entry.query=";
    anno.entry.query->Dump(os);
    os << ",filters=" << dump_set(anno.filters);
    os << ')';
    return os.str();
  }

  static std::string dump_anno_entries(const AnnotatedList& entries) {
    std::ostringstream os;
    os << "(sz=" << entries.size() << ',';
    bool first = true;
    for (const auto& anno : entries) {
      if (first) {
        first = false;
      } else {
        os << ',';
      }
      os << dump_anno(anno);
    }
    os << ")";
    return os.str();
  }

  static std::shared_ptr<QueryIndex<T>> create_impl(
      AnnotatedIdxMap* idx_map, const AnnotatedList& entries) {
    const auto& maybe_idx = idx_map->find(entries);
    if (maybe_idx != idx_map->end()) {
      return maybe_idx->second;
    }

    AnnotatedList children;
    AnnotatedList leaf;
    std::partition_copy(entries.begin(), entries.end(),
                        std::back_inserter(children), std::back_inserter(leaf),
                        [](const QueryIndexAnnotated<T>& entry) {
                          return !entry.filters.empty();
                        });

    IndexMap trees;

    ska::flat_hash_map<std::shared_ptr<RelopQuery>, AnnotatedList> grouped_by_q;
    for (const auto& entry : children) {
      auto query_anno_list = entry.to_query_annotated_list();
      for (const auto& query_anno : query_anno_list) {
        grouped_by_q[query_anno.first].emplace_back(query_anno.second);
      }
    }

    for (const auto& q_lst : grouped_by_q) {
      trees[q_lst.first] = create_impl(idx_map, q_lst.second);
    }

    EntryList leaf_entries;
    leaf_entries.reserve(leaf.size());
    std::transform(leaf.begin(), leaf.end(), std::back_inserter(leaf_entries),
                   [](const QueryIndexAnnotated<T>& a) { return a.entry; });
    auto query_index = std::make_shared<QueryIndex>(trees, leaf_entries);
    idx_map->insert(std::make_pair(entries, query_index));
    return query_index;
  }

  std::unordered_set<T> slow_matches(const meter::Tags& tags) const noexcept {
    std::unordered_set<T> res;
    for (const auto& e : entries_) {
      if (e.query->Matches(tags)) {
        res.emplace(e.value);
      }
    }
    return res;
  }

  std::unordered_set<T> matching_entries(const meter::Tags& tags,
                                         RelopQueries::const_iterator begin,
                                         RelopQueries::const_iterator end) const
      noexcept {
    if (begin == end) {
      return slow_matches(tags);
    } else {
      auto& q = *begin;
      begin++;
      std::unordered_set<T> children;
      const auto& matching = indexes_.find(q);
      if (matching != indexes_.end()) {
        children = matching->second->matching_entries(tags, begin, end);
      }
      append_to_set(&children, slow_matches(tags));
      append_to_set(&children, matching_entries(tags, begin, end));
      return children;
    }
  }

  // Split :in queries into a list of queries using :eq.
  static Queries split(QueryPtr q) {
    Queries result;
    auto t = q->GetQueryType();
    if (t == QueryType::And) {
      auto and_q = std::static_pointer_cast<AndQuery>(q);
      auto q1 = split(and_q->q1_);
      auto q2 = split(and_q->q2_);
      return vector_cross_product(q1, q2, query::and_q);
    } else if (t == QueryType::In) {
      auto in_q = std::static_pointer_cast<InQuery>(q);
      const auto& vs = in_q->Values();
      // avoid combinatorial explosion with in queries with too many values
      if (vs.size() < 5) {
        auto k = in_q->KeyRef();
        for (auto& v : vs) {
          result.emplace_back(std::make_shared<RelopQuery>(k, v, RelOp::EQ));
        }
      } else {
        result.emplace_back(q);
      }
    } else {
      result.emplace_back(q);
    }
    return result;
  }

  /// Convert a query into a list of query clauses that are ANDd together.
  static Queries conjunction_list(const QueryPtr& query) {
    if (query->GetQueryType() == QueryType::And) {
      auto and_q = std::static_pointer_cast<AndQuery>(query);
      return vector_concat(conjunction_list(and_q->q1_),
                           conjunction_list(and_q->q2_));
    }

    Queries res;
    res.emplace_back(query);
    return res;
  }

  /// Annotate an entry with a set of :eq queries that should filter in the
  /// input before checking against the final remaining query. Ideally if the
  /// query is only using :eq and :and the final remainder will be :true.
  static QueryIndexAnnotated<T> annotate(const QueryIndexEntry<T>& entry) {
    auto queries = conjunction_list(entry.query);
    ska::flat_hash_set<QueryPtr> distinct{queries.begin(), queries.end()};
    ska::flat_hash_set<std::shared_ptr<RelopQuery>> filters;
    Queries remainder;
    for (auto& q : distinct) {
      if (q->GetQueryType() == QueryType::RelOp) {
        auto relop = std::static_pointer_cast<RelopQuery>(q);
        if (relop->GetRelationalOperator() == RelOp::EQ) {
          filters.emplace(relop);
        } else {
          remainder.emplace_back(q);
        }
      } else {
        remainder.emplace_back(q);
      }
    }
    QueryPtr remainder_query =
        remainder.empty() ? query::true_q() : query::and_queries(remainder);
    return QueryIndexAnnotated<T>(
        QueryIndexEntry<T>{remainder_query, entry.value}, std::move(filters));
  }
};

}  // namespace interpreter
}  // namespace atlas
