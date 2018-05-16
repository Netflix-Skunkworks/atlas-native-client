#include "expression.h"
#include "../util/logger.h"
#include "query.h"

namespace atlas {
namespace interpreter {

using meter::Measurements;
using util::intern_str;
using util::Logger;

Literal::Literal(std::string str) noexcept : str_(std::move(str)) {}

bool Literal::Is(const std::string& str) const noexcept { return str_ == str; }

std::ostream& Literal::Dump(std::ostream& os) const {
  os << "Literal(" << str_ << ")";
  return os;
}

bool Literal::IsWord() const noexcept { return str_[0] == ':'; }

const std::string Literal::GetWord() const {
  if (!this->IsWord()) {
    Logger()->error("Internal error: GetWord() called on non-word: ", *this);
  }
  return str_.substr(1);
}

std::string Literal::AsString() const noexcept { return str_; }

std::ostream& ConstantExpression::Dump(std::ostream& os) const {
  os << "ConstantExpression(" << value_ << ")";
  return os;
}

ConstantExpression::ConstantExpression(double value) noexcept : value_(value) {}

std::shared_ptr<Query> ConstantExpression::GetQuery() const noexcept {
  return query::false_q();
}

std::ostream& List::Dump(std::ostream& os) const {
  os << "List(";
  bool first = true;
  for (const auto& elt : list_) {
    if (!first) {
      os << ",";
    } else {
      first = false;
    }
    elt->Dump(os);
  }
  os << ")";
  return os;
}

void List::Add(const std::shared_ptr<Expression>& expression) {
  list_.push_back(expression);
}

bool List::Contains(const std::string& key) const noexcept {
  auto contains_key = [&key](const std::shared_ptr<Expression>& s) {
    return expression::Is(*s, key);
  };
  return std::any_of(list_.begin(), list_.end(), contains_key);
}

std::unique_ptr<StringRefs> List::ToStrings() const {
  auto strs = std::make_unique<StringRefs>();
  strs->reserve(list_.size());
  for (auto& e : list_) {
    if (expression::IsLiteral(*e)) {
      strs->push_back(intern_str(expression::LiteralToStr(*e)));
    }
  }
  return strs;
}

size_t List::Size() const noexcept { return list_.size(); }

std::ostream& operator<<(std::ostream& os, const Expression& expression) {
  expression.Dump(os);
  return os;
}

}  // namespace interpreter
}  // namespace atlas
