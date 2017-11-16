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

  std::shared_ptr<TagsValuePairs> Apply(std::shared_ptr<TagsValuePairs> tagsValuePairs) override;

  virtual std::ostream& Dump(std::ostream& os) const override;

 private:
  std::unique_ptr<StringRefs> keys_;
  std::shared_ptr<ValueExpression> expr_;
};

namespace expression {
std::shared_ptr<MultipleResults> GetMultipleResults(
    std::shared_ptr<Expression> e);
}  // namespace expression
}  // namespace interpreter
}  // namespace atlas
