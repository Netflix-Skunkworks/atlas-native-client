#pragma once

#include "context.h"
#include "multiple_results.h"

namespace atlas {
namespace interpreter {

class All : public MultipleResults {
 public:
  explicit All(std::shared_ptr<Query> query);

  ExpressionType GetType() const noexcept override {
    return ExpressionType::MultipleResults;
  }

  std::shared_ptr<Query> GetQuery() const noexcept override { return query_; }

  TagsValuePairs Apply(const TagsValuePairs& measurements) override;

  std::ostream& Dump(std::ostream& os) const override;

 private:
  std::shared_ptr<Query> query_;
};
}  // namespace interpreter
}  // namespace atlas
