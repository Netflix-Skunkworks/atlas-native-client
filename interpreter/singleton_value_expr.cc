#include "singleton_value_expr.h"

namespace atlas {
namespace interpreter {

SingletonValueExpr::SingletonValueExpr(std::shared_ptr<ValueExpression> expr)
    : expr_(std::move(expr)) {}

TagsValuePairs SingletonValueExpr::Apply(const TagsValuePairs& measurements) {
  auto expr_result = expr_->Apply(measurements);
  auto v = expr_result->value();
  if (std::isnan(v)) {
    return {};
  }
  return {TagsValuePair::of(expr_result->all_tags(), v)};
}

std::ostream& SingletonValueExpr::Dump(std::ostream& os) const {
  os << "SingletonVE{expr=" << *expr_ << "}";
  return os;
}

}  // namespace interpreter
}  // namespace atlas