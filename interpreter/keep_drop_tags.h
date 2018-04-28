#pragma once

#include "multiple_results.h"

namespace atlas {
namespace interpreter {
class KeepOrDropTags : public MultipleResults {
 public:
  KeepOrDropTags(const List& keys, std::shared_ptr<ValueExpression> expr,
                 bool keep);

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
  bool keep_;
};
}  // namespace interpreter
}  // namespace atlas
