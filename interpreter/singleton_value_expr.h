#pragma once

#include "multiple_results.h"

namespace atlas {
namespace interpreter {

class SingletonValueExpr : public MultipleResults {
 public:
  explicit SingletonValueExpr(std::shared_ptr<ValueExpression>);

  ExpressionType GetType() const noexcept override {
    return ExpressionType::MultipleResults;
  }

  std::shared_ptr<Query> GetQuery() const noexcept override {
    return expr_->GetQuery();
  }

  TagsValuePairs Apply(const TagsValuePairs& measurements) override;

  std::ostream& Dump(std::ostream& os) const override;

 private:
  std::shared_ptr<ValueExpression> expr_;
};

}  // namespace interpreter
}  // namespace atlas
