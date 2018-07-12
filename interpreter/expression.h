#pragma once

#include "tags_value.h"
#include <cmath>
#include <forward_list>
#include <spdlog/fmt/ostr.h>

namespace atlas {
namespace interpreter {

using StringRefs = std::vector<util::StrRef>;

enum class ExpressionType {
  Literal,
  List,
  Query,
  ValueExpression,
  MultipleResults
};

class Expression {
 public:
  virtual ExpressionType GetType() const noexcept = 0;

  virtual std::ostream& Dump(std::ostream& os) const = 0;

  virtual ~Expression() noexcept = default;
};

std::ostream& operator<<(std::ostream& os, const Expression& expression);

class Literal : public Expression {
 public:
  explicit Literal(std::string str) noexcept;

  std::string AsString() const noexcept;

  bool Is(const std::string& str) const noexcept;

  bool IsWord() const noexcept;

  const std::string GetWord() const;

  std::ostream& Dump(std::ostream& os) const override;

  ExpressionType GetType() const noexcept override {
    return ExpressionType::Literal;
  }

 private:
  const std::string str_;
};

class List : public Expression {
 public:
  List() = default;

  std::ostream& Dump(std::ostream& os) const override;

  void Add(const std::shared_ptr<Expression>& expression);

  bool Contains(const std::string& key) const noexcept;

  std::unique_ptr<StringRefs> ToStrings() const;

  size_t Size() const noexcept;

  ExpressionType GetType() const noexcept override {
    return ExpressionType::List;
  }

 private:
  std::vector<std::shared_ptr<Expression>> list_;
};

class Query;

class FalseQuery;
class ValueExpression : public Expression {
 public:
  virtual std::unique_ptr<TagsValuePair> Apply(
      const TagsValuePairs& measurements) const = 0;

  virtual std::shared_ptr<Query> GetQuery() const noexcept = 0;

  ExpressionType GetType() const noexcept override {
    return ExpressionType::ValueExpression;
  }
};

class ConstantExpression : public ValueExpression {
 public:
  explicit ConstantExpression(double value) noexcept;

  std::unique_ptr<TagsValuePair> Apply(const TagsValuePairs&) const override {
    return TagsValuePair::of(meter::Tags{}, value_);
  };

  std::shared_ptr<Query> GetQuery() const noexcept override;

  std::ostream& Dump(std::ostream& os) const override;

 private:
  const double value_;
};

namespace expression {
inline bool IsLiteral(const Expression& e) noexcept {
  return e.GetType() == ExpressionType::Literal;
}

inline bool IsList(const Expression& e) noexcept {
  return e.GetType() == ExpressionType::List;
}

inline bool IsQuery(const Expression& e) noexcept {
  return e.GetType() == ExpressionType::Query;
}

inline bool Is(const Expression& e, const std::string& s) noexcept {
  return IsLiteral(e) && static_cast<const Literal&>(e).AsString() == s;
}

inline bool IsWord(const Expression& e) noexcept {
  return IsLiteral(e) && static_cast<const Literal&>(e).IsWord();
}

inline const std::string GetWord(const Expression& e) noexcept {
  return static_cast<const Literal&>(e).GetWord();
}

inline const std::string LiteralToStr(const Expression& e) noexcept {
  return static_cast<const Literal&>(e).AsString();
}

}  // namespace expression
}  // namespace interpreter
}  // namespace atlas
