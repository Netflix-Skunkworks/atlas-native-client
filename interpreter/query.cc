#include "query.h"
#include "../meter/id.h"
#include "../util/logger.h"

namespace atlas {
namespace interpreter {

using util::intern_str;
using util::Logger;
using util::StrRef;

bool Query::IsTrue() const noexcept { return false; }

bool Query::IsFalse() const noexcept { return false; }

bool Query::IsRegex() const noexcept { return false; }

HasKeyQuery::HasKeyQuery(std::string key) : AbstractKeyQuery(std::move(key)) {}

bool HasKeyQuery::Matches(const meter::Tags& tags) const {
  return tags.has(KeyRef());
}

bool HasKeyQuery::Matches(const TagsValuePair& tv) const {
  // true if it has a value for KeyRef
  return tv.get_value(KeyRef());
}

std::ostream& HasKeyQuery::Dump(std::ostream& os) const {
  os << "HasKey(" << Key() << ")";
  return os;
}

AbstractKeyQuery::AbstractKeyQuery(std::string key) noexcept
    : key_(std::move(key)) {}

const OptionalString AbstractKeyQuery::getvalue(const meter::Tags& tags) const
    noexcept {
  auto k = tags.at(KeyRef());
  if (*k.get() == '\0') {
    return kNone;
  }
  return OptionalString{k.get()};
}

const std::string& AbstractKeyQuery::Key() const noexcept { return key_; }

StrRef AbstractKeyQuery::KeyRef() const noexcept { return intern_str(key_); }

RelopQuery::RelopQuery(std::string k, std::string v, RelOp op)
    : AbstractKeyQuery(std::move(k)), op_(op), value_(std::move(v)) {}

bool RelopQuery::Matches(const meter::Tags& tags) const {
  auto current_value = getvalue(tags);
  return do_query(current_value, value_, op_);
}

bool RelopQuery::Matches(const TagsValuePair& tv) const {
  auto current_value = tv.get_value(KeyRef());
  return do_query(current_value, value_, op_);
}

bool RelopQuery::do_query(const OptionalString& cur_value, const std::string& v,
                          RelOp op) {
  if (!cur_value) {
    return false;
  }

  switch (op) {
    case RelOp::EQ:
      return cur_value == v;
    case RelOp::LE:
      return cur_value <= v;
    case RelOp::LT:
      return cur_value < v;
    case RelOp::GE:
      return cur_value >= v;
    case RelOp::GT:
      return cur_value > v;
  }

  // unreachable - but gets rid of a warning in trusty
  return false;
}

std::ostream& RelopQuery::Dump(std::ostream& os) const {
  os << "RelopQuery(" << Key() << op_ << value_ << ")";
  return os;
}

meter::Tags RelopQuery::Tags() const noexcept {
  if (op_ == RelOp::EQ) {
    return meter::Tags{{KeyRef(), intern_str(value_)}};
  }
  return meter::Tags();
}

static pcre* get_pattern(const std::string& v, bool ignore_case) {
  const char* error;
  pcre* re;
  int error_offset;
  int options = PCRE_ANCHORED;
  if (ignore_case) {
    options |= PCRE_CASELESS;
  }
  re = pcre_compile(v.c_str(), options, &error, &error_offset, nullptr);
  if (re == nullptr) {
    Logger()->warn("Invalid regex: {} {}", v, error);
  }

  return re;
}

RegexQuery::RegexQuery(std::string k, const std::string& pattern,
                       bool ignore_case)
    : AbstractKeyQuery(std::move(k)),
      pattern(get_pattern(pattern, ignore_case)),
      str_pattern(pattern) {}

static constexpr size_t kOffsetsMax = 30;

static bool MatchesRegex(pcre* pattern, const std::string& str_pattern,
                         const OptionalString& value) {
  if (pattern == nullptr) {
    return false;
  }

  if (!value) {
    return false;
  }

  int offsets[kOffsetsMax];
  auto rc =
      pcre_exec(pattern, nullptr, value->c_str(),
                static_cast<int>(value->length()), 0, 0, offsets, kOffsetsMax);
  if (rc >= 0) {
    return true;
  }
  if (rc == PCRE_ERROR_NOMATCH) {
    return false;
  }
  const char* error_msg;
  switch (rc) {
    case PCRE_ERROR_NULL:
      error_msg = "Something was null in PCRE";
      break;
    case PCRE_ERROR_BADOPTION:
      error_msg = "Bad PCRE option passed";
      break;
    case PCRE_ERROR_BADMAGIC:
    case PCRE_ERROR_UNKNOWN_NODE:
      error_msg = "Compiled RE is corrupt";
      break;
    default:
      error_msg = "Unknown error executing regular expression.";
  }
  Logger()->error("Error executing regular expression {} against {}: {}",
                  str_pattern, *value, error_msg);
  return false;
}

bool RegexQuery::Matches(const meter::Tags& tags) const {
  auto value = getvalue(tags);
  return MatchesRegex(pattern, str_pattern, value);
}

bool RegexQuery::Matches(const TagsValuePair& tv) const {
  auto value = tv.get_value(KeyRef());
  return MatchesRegex(pattern, str_pattern, value);
}

std::ostream& RegexQuery::Dump(std::ostream& os) const {
  os << "RegexQuery(" << Key() << " ~ " << str_pattern << "])";
  return os;
}

bool RegexQuery::IsRegex() const noexcept { return true; }

RegexQuery::~RegexQuery() {
  if (pattern != nullptr) {
    pcre_free(pattern);
  }
}

std::ostream& operator<<(std::ostream& os, const RelOp& op) {
  switch (op) {
    case RelOp::EQ:
      os << "=";
      break;
    case RelOp::GT:
      os << ">";
      break;
    case RelOp::GE:
      os << ">=";
      break;
    case RelOp::LT:
      os << "<";
      break;
    case RelOp::LE:
      os << "<=";
      break;
  }
  return os;
}

InQuery::InQuery(std::string key, std::unique_ptr<StringRefs> vs) noexcept
    : AbstractKeyQuery(std::move(key)), vs_(std::move(vs)) {}

bool InQuery::matches_value(const OptionalString& value) const {
  if (!value) {
    return false;
  }
  auto valueRef = intern_str(value.get());
  return std::any_of(vs_->begin(), vs_->end(),
                     [valueRef](util::StrRef& s) { return valueRef == s; });
}

bool InQuery::Matches(const meter::Tags& tags) const {
  auto value = getvalue(tags);
  return matches_value(value);
}

bool InQuery::Matches(const TagsValuePair& tv) const {
  auto value = tv.get_value(KeyRef());
  return matches_value(value);
}

std::ostream& InQuery::Dump(std::ostream& os) const { return os; }

std::ostream& TrueQuery::Dump(std::ostream& os) const {
  os << "TrueQuery";
  return os;
}

std::ostream& FalseQuery::Dump(std::ostream& os) const {
  os << "FalseQuery";
  return os;
}

NotQuery::NotQuery(std::shared_ptr<Query> query) : query_(std::move(query)) {
  if (!query_) {
    Logger()->error(
        "Internal error in :not. Got a null pointer as a query to negate.");
  }
}

bool NotQuery::Matches(const meter::Tags& tags) const {
  return !query_->Matches(tags);
}

bool NotQuery::Matches(const TagsValuePair& tv) const {
  return !query_->Matches(tv);
}

std::ostream& NotQuery::Dump(std::ostream& os) const {
  os << "NotQuery(" << *query_ << ")";
  return os;
}

AndQuery::AndQuery(std::shared_ptr<Query> q1, std::shared_ptr<Query> q2)
    : q1_(std::move(q1)), q2_(std::move(q2)) {}

std::ostream& AndQuery::Dump(std::ostream& os) const {
  os << "AndQuery(" << *q1_ << ", " << *q2_ << ")";
  return os;
}

bool AndQuery::Matches(const meter::Tags& tags) const {
  return q1_->Matches(tags) && q2_->Matches(tags);
}

bool AndQuery::Matches(const TagsValuePair& tv) const {
  return q1_->Matches(tv) && q2_->Matches(tv);
}

meter::Tags AndQuery::Tags() const noexcept {
  auto t1 = q1_->Tags();
  auto t2 = q2_->Tags();
  t1.add_all(t2);
  return t1;
}

OrQuery::OrQuery(std::shared_ptr<Query> q1, std::shared_ptr<Query> q2)
    : q1_(std::move(q1)), q2_(std::move(q2)) {}

std::ostream& OrQuery::Dump(std::ostream& os) const {
  os << "OrQuery(" << *q1_ << ", " << *q2_ << ")";
  return os;
}

bool OrQuery::Matches(const meter::Tags& tags) const {
  return q1_->Matches(tags) || q2_->Matches(tags);
}

bool OrQuery::Matches(const TagsValuePair& tv) const {
  return q1_->Matches(tv) || q2_->Matches(tv);
}

// helper functions
std::unique_ptr<Query> query::not_q(std::shared_ptr<Query> q) noexcept {
  if (q->IsFalse()) {
    return std::make_unique<TrueQuery>();
  }
  if (q->IsTrue()) {
    return std::make_unique<FalseQuery>();
  }
  return std::make_unique<NotQuery>(q);
}

std::shared_ptr<Query> query::or_q(std::shared_ptr<Query> q1,
                                   std::shared_ptr<Query> q2) noexcept {
  if (q1->IsTrue()) {
    return q1;
  }
  if (q2->IsTrue()) {
    return q2;
  }
  if (q1->IsFalse()) {
    return q2;
  }
  if (q2->IsFalse()) {
    return q1;
  }
  if (*q1 == *q2) {
    return q1;
  }
  if (q1->IsRegex()) {
    return std::make_shared<OrQuery>(q2, q1);
  }
  return std::make_shared<OrQuery>(q1, q2);
}

std::shared_ptr<Query> query::and_q(std::shared_ptr<Query> q1,
                                    std::shared_ptr<Query> q2) noexcept {
  if (q1->IsFalse()) {
    return q1;
  }
  if (q2->IsFalse()) {
    return q2;
  }
  if (q1->IsTrue()) {
    return q2;
  }
  if (q2->IsTrue()) {
    return q1;
  }
  if (*q1 == *q2) {
    return q1;
  }
  if (q1->IsRegex()) {
    return std::make_shared<AndQuery>(q2, q1);
  }
  return std::make_shared<AndQuery>(q1, q2);
}
}  // namespace interpreter
}  // namespace atlas
