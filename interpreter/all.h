#pragma once

#include "context.h"
#include "multiple_results.h"

namespace atlas {
namespace interpreter {

class All : public MultipleResults {
 public:
  explicit All(std::shared_ptr<ValueExpression> expr);

  ExpressionType GetType() const noexcept override {
    return ExpressionType::MultipleResults;
  }

  std::shared_ptr<Query> GetQuery() const noexcept override {
    return expr_->GetQuery();
  }

  std::shared_ptr<TagsValuePairs> Apply(std::shared_ptr<TagsValuePairs> measurements) override;

  virtual std::ostream& Dump(std::ostream& os) const override;

 private:
  std::shared_ptr<ValueExpression> expr_;
};
}  // namespace interpreter
}  // namespace atlas
