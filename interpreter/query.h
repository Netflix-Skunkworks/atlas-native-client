#pragma once

#include "../meter/id.h"
#include "../util/optional.h"
#include "expression.h"
#include <pcre.h>

namespace atlas {
namespace interpreter {

enum class QueryType { HasKey, RelOp, Regex, In, True, False, Not, Or, And };

class Query : public Expression {
 public:
  ExpressionType GetType() const noexcept override {
    return ExpressionType::Query;
  }

  virtual bool Matches(const meter::Tags& tags) const = 0;

  virtual bool Matches(const TagsValuePair& tv) const = 0;

  virtual meter::Tags Tags() const noexcept { return meter::Tags(); };

  virtual size_t Hash() const noexcept = 0;

  virtual bool Equals(const Query& query) const noexcept = 0;

  virtual QueryType GetQueryType() const noexcept = 0;

  bool IsFalse() const noexcept { return QueryType::False == GetQueryType(); }

  bool IsTrue() const noexcept { return QueryType::True == GetQueryType(); }
};

inline bool operator==(const Query& q1, const Query& q2) {
  return q1.Equals(q2);
}

class AbstractKeyQuery : public Query {
 public:
  explicit AbstractKeyQuery(util::StrRef key_ref) noexcept;

  const char* Key() const noexcept;

  util::StrRef KeyRef() const noexcept;

 protected:
  util::StrRef key_ref_;
  OptionalString getvalue(const meter::Tags& tags) const noexcept;
};

class HasKeyQuery : public AbstractKeyQuery {
 public:
  explicit HasKeyQuery(util::StrRef key_ref);
  explicit HasKeyQuery(const char* key);

  std::ostream& Dump(std::ostream& os) const override;

  bool Matches(const meter::Tags& tags) const override;

  bool Matches(const TagsValuePair& tv) const override;

  QueryType GetQueryType() const noexcept override { return QueryType::HasKey; }

  bool Equals(const Query& query) const noexcept override {
    if (query.GetQueryType() != GetQueryType()) return false;

    const auto& q = static_cast<const HasKeyQuery&>(query);
    return KeyRef() == q.KeyRef();
  }

  size_t Hash() const noexcept override {
    auto n = static_cast<size_t>(GetQueryType());
    auto h = std::hash<util::StrRef>()(KeyRef());
    return (n << 16u) ^ h;
  }
};

enum class RelOp { GT, LT, GE, LE, EQ };

std::ostream& operator<<(std::ostream& os, const RelOp& op);

class RelopQuery : public AbstractKeyQuery {
 public:
  RelopQuery(util::StrRef k, util::StrRef v, RelOp op) noexcept;

  std::ostream& Dump(std::ostream& os) const override;

  bool Matches(const meter::Tags& tags) const override;

  bool Matches(const TagsValuePair& tv) const override;

  static bool do_query(const OptionalString& cur_value, util::StrRef v,
                       RelOp op);

  meter::Tags Tags() const noexcept override;

  bool Equals(const Query& query) const noexcept override {
    if (query.GetQueryType() != GetQueryType()) return false;

    const auto& q = static_cast<const RelopQuery&>(query);
    return KeyRef() == q.KeyRef() && op_ == q.op_ && value_ref_ == q.value_ref_;
  }

  size_t Hash() const noexcept override {
    auto n = static_cast<size_t>(GetQueryType());
    auto h = std::hash<util::StrRef>()(KeyRef());
    auto v = std::hash<util::StrRef>()(ValueRef());
    return (n << 16u) ^ h ^ v;
  }

  QueryType GetQueryType() const noexcept override { return QueryType::RelOp; }

  util::StrRef ValueRef() const noexcept { return value_ref_; }

  RelOp GetRelationalOperator() const noexcept { return op_; }

 private:
  RelOp op_;
  util::StrRef value_ref_;
};

class RegexQuery : public AbstractKeyQuery {
 public:
  RegexQuery(const char* key, const std::string& pattern, bool ignore_case);
  RegexQuery(util::StrRef key_ref, const std::string& pattern,
             bool ignore_case);
  ~RegexQuery() override;

  bool Matches(const meter::Tags& tags) const override;

  bool Matches(const TagsValuePair& tv) const override;

  std::ostream& Dump(std::ostream& os) const override;

  bool Equals(const Query& query) const noexcept override {
    if (query.GetQueryType() != GetQueryType()) return false;

    const auto& q = static_cast<const RegexQuery&>(query);
    return Key() == q.Key() && ignore_case_ == q.ignore_case_ &&
           str_pattern == q.str_pattern;
  }

  size_t Hash() const noexcept override {
    auto n = static_cast<size_t>(GetQueryType());
    auto k = std::hash<util::StrRef>()(KeyRef());
    auto v = std::hash<std::string>()(str_pattern);
    return (n << 16) ^ k ^ v;
  }

  QueryType GetQueryType() const noexcept override { return QueryType::Regex; }

 private:
  pcre* pattern;
  const std::string str_pattern;
  bool ignore_case_;
};

class InQuery : public AbstractKeyQuery {
 public:
  InQuery(util::StrRef key, std::unique_ptr<StringRefs> vs) noexcept;

  std::ostream& Dump(std::ostream& os) const override;

  bool Matches(const meter::Tags& tags) const override;

  bool Matches(const TagsValuePair& tv) const override;

  bool Equals(const Query& query) const noexcept override {
    if (query.GetQueryType() != GetQueryType()) return false;

    const auto& q = static_cast<const InQuery&>(query);
    return KeyRef() == q.KeyRef() && *vs_ == *(q.vs_);
  }

  size_t Hash() const noexcept override {
    auto n = static_cast<size_t>(GetQueryType());
    auto res = (n << 16) ^ std::hash<util::StrRef>()(KeyRef());
    for (auto& v : *vs_) {
      res ^= std::hash<util::StrRef>()(v);
    }
    return res;
  }

  const StringRefs& Values() const noexcept { return *vs_; }

  QueryType GetQueryType() const noexcept override { return QueryType::In; }

 private:
  std::unique_ptr<StringRefs> vs_;
  bool matches_value(const OptionalString& value) const;
};

class TrueQuery : public Query {
 public:
  TrueQuery() noexcept {};

  std::ostream& Dump(std::ostream& os) const override;

  bool Matches(const meter::Tags&) const override { return true; }

  bool Matches(const TagsValuePair&) const override { return true; }

  bool Equals(const Query& query) const noexcept override {
    return query.GetQueryType() == QueryType::True;
  }

  size_t Hash() const noexcept override {
    return static_cast<size_t>(GetQueryType());
  }

  QueryType GetQueryType() const noexcept override { return QueryType::True; }
};

class FalseQuery : public Query {
 public:
  FalseQuery() noexcept {};

  std::ostream& Dump(std::ostream& os) const override;

  bool Matches(const meter::Tags&) const override { return false; }

  bool Matches(const TagsValuePair&) const override { return false; }

  bool Equals(const Query& query) const noexcept override {
    return query.GetQueryType() == QueryType::False;
  }

  size_t Hash() const noexcept override {
    return static_cast<size_t>(GetQueryType());
  }

  QueryType GetQueryType() const noexcept override { return QueryType::False; }
};

class NotQuery : public Query {
 public:
  explicit NotQuery(std::shared_ptr<Query> query);

  bool Matches(const meter::Tags& tags) const override;

  bool Matches(const TagsValuePair& tv) const override;

  std::ostream& Dump(std::ostream& os) const override;

  bool Equals(const Query& query) const noexcept override {
    if (query.GetQueryType() != GetQueryType()) return false;
    const auto& q = static_cast<const NotQuery&>(query);
    return query_->Equals(*(q.query_));
  }

  size_t Hash() const noexcept override { return query_->Hash() - 1; }

  QueryType GetQueryType() const noexcept override { return QueryType::Not; }

  std::shared_ptr<Query> query_;
};

class OrQuery : public Query {
 public:
  OrQuery(std::shared_ptr<Query> q1, std::shared_ptr<Query> q2);

  std::ostream& Dump(std::ostream& os) const override;

  bool Matches(const meter::Tags& tags) const override;

  bool Matches(const TagsValuePair& tv) const override;

  bool Equals(const Query& query) const noexcept override {
    if (query.GetQueryType() != QueryType::Or) return false;
    const auto& q = static_cast<const OrQuery&>(query);
    return (q.q1_->Equals(*q1_) && q.q2_->Equals(*q2_)) ||
           (q.q2_->Equals(*q1_) && q.q1_->Equals(*q2_));
  }

  size_t Hash() const noexcept override {
    return q1_->Hash() ^ q2_->Hash() ^ static_cast<size_t>(GetQueryType());
  }

  QueryType GetQueryType() const noexcept override { return QueryType::Or; }

  std::shared_ptr<Query> q1_;
  std::shared_ptr<Query> q2_;
};

class AndQuery : public Query {
 public:
  AndQuery(std::shared_ptr<Query> q1, std::shared_ptr<Query> q2);

  meter::Tags Tags() const noexcept override;

  std::ostream& Dump(std::ostream& os) const override;

  bool Matches(const meter::Tags& tags) const override;

  bool Matches(const TagsValuePair& tv) const override;

  bool Equals(const Query& query) const noexcept override {
    if (query.GetQueryType() != QueryType::And) return false;
    const auto& q = static_cast<const AndQuery&>(query);
    // a and b == b and a
    return (q.q1_->Equals(*q1_) && q.q2_->Equals(*q2_)) ||
           (q.q1_->Equals(*q2_) && q.q2_->Equals(*q1_));
  }

  size_t Hash() const noexcept override {
    return q1_->Hash() ^ q2_->Hash() ^ static_cast<size_t>(GetQueryType());
  }

  QueryType GetQueryType() const noexcept override { return QueryType::And; }

  std::shared_ptr<Query> q1_;
  std::shared_ptr<Query> q2_;
};
// helper functions to create queries
namespace query {

inline std::unique_ptr<Query> eq(const char* k, const char* v) noexcept {
  return std::make_unique<RelopQuery>(util::intern_str(k), util::intern_str(v),
                                      RelOp::EQ);
}

inline std::unique_ptr<Query> in(const char* k, StringRefs vs) noexcept {
  auto unique_vs = std::make_unique<StringRefs>(std::move(vs));
  return std::make_unique<InQuery>(util::intern_str(k), std::move(unique_vs));
}

inline std::unique_ptr<Query> re(const char* k, std::string pattern) noexcept {
  return std::make_unique<RegexQuery>(util::intern_str(k), std::move(pattern),
                                      false);
}

inline std::unique_ptr<Query> reic(const char* k,
                                   std::string pattern) noexcept {
  return std::make_unique<RegexQuery>(util::intern_str(k), std::move(pattern),
                                      true);
}

inline std::unique_ptr<Query> gt(const char* k, const char* v) noexcept {
  return std::make_unique<RelopQuery>(util::intern_str(k), util::intern_str(v),
                                      RelOp::GT);
}

inline std::unique_ptr<Query> ge(const char* k, const char* v) noexcept {
  return std::make_unique<RelopQuery>(util::intern_str(k), util::intern_str(v),
                                      RelOp::GE);
}

inline std::unique_ptr<Query> lt(const char* k, const char* v) noexcept {
  return std::make_unique<RelopQuery>(util::intern_str(k), util::intern_str(v),
                                      RelOp::LT);
}

inline std::unique_ptr<Query> le(const char* k, const char* v) noexcept {
  return std::make_unique<RelopQuery>(util::intern_str(k), util::intern_str(v),
                                      RelOp::LE);
}

inline std::unique_ptr<Query> true_q() noexcept {
  return std::make_unique<TrueQuery>();
}

inline std::unique_ptr<Query> false_q() noexcept {
  return std::make_unique<FalseQuery>();
}

inline std::unique_ptr<Query> from_boolean(bool b) noexcept {
  return b ? false_q() : true_q();
}

std::unique_ptr<Query> not_q(std::shared_ptr<Query> q) noexcept;

std::shared_ptr<Query> or_q(std::shared_ptr<Query> q1,
                            std::shared_ptr<Query> q2) noexcept;

std::shared_ptr<Query> and_q(std::shared_ptr<Query> q1,
                             std::shared_ptr<Query> q2) noexcept;

std::shared_ptr<Query> and_queries(
    std::vector<std::shared_ptr<Query>> queries) noexcept;

/// Convert the input query into a list of sub-queris that should be ORd
/// together
std::vector<std::shared_ptr<Query>> dnf_list(std::shared_ptr<Query> query);

}  // namespace query
}  // namespace interpreter
}  // namespace atlas

namespace std {
template <>
struct hash<atlas::interpreter::RelopQuery> {
  size_t operator()(const atlas::interpreter::RelopQuery& query) const {
    auto k = std::hash<atlas::util::StrRef>()(query.KeyRef());
    auto v = std::hash<atlas::util::StrRef>()(query.ValueRef());
    return k ^ v;
  }
};

template <>
struct equal_to<atlas::interpreter::RelopQuery> {
  bool operator()(const atlas::interpreter::RelopQuery& lhs,
                  const atlas::interpreter::RelopQuery& rhs) const {
    return lhs.Equals(rhs);
  }
};

}  // namespace std