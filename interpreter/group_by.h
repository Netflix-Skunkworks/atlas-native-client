#pragma once

#include "all.h"
#include "context.h"
#include "expression.h"

namespace atlas {
namespace interpreter {

class GroupBy : public MultipleResults {
 public:
  GroupBy(const List& keys, std::shared_ptr<ValueExpression> expr);

  ExpressionType GetType() const noexcept override {
    return ExpressionType::MultipleResults;
  }

  std::shared_ptr<Query> GetQuery() const noexcept override {
    return expr_->GetQuery();
  }

  TagsValuePairs Apply(const TagsValuePairs& measurements) override;

  std::ostream& Dump(std::ostream& os) const override;

 private:
  std::unique_ptr<StringRefs> keys_;
  std::shared_ptr<ValueExpression> expr_;
};

std::shared_ptr<MultipleResults> GetMultipleResultsExpr(
    std::shared_ptr<Expression> e);

}  // namespace interpreter
}  // namespace atlas
