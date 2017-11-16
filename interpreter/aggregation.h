#pragma once

#include "expression.h"
#include "query.h"

namespace atlas {
namespace interpreter {

enum class Aggregate { COUNT, SUM, MAX, MIN, AVG };

class AggregateExpression : public ValueExpression {
 public:
  AggregateExpression(Aggregate aggregate, std::shared_ptr<Query> filter);

  virtual std::ostream& Dump(std::ostream& os) const override;

  virtual std::unique_ptr<TagsValuePair> Apply(
      const TagsValuePairs& tagsValuePairs) const override;

  std::shared_ptr<Query> GetQuery() const noexcept override { return filter_; }

 private:
  Aggregate aggregate_;
  std::shared_ptr<Query> filter_;
};

namespace aggregation {

std::unique_ptr<AggregateExpression> count(std::shared_ptr<Query> filter);

std::unique_ptr<AggregateExpression> sum(std::shared_ptr<Query> filter);

std::unique_ptr<AggregateExpression> max(std::shared_ptr<Query> filter);

std::unique_ptr<AggregateExpression> min(std::shared_ptr<Query> filter);

std::unique_ptr<AggregateExpression> avg(std::shared_ptr<Query> filter);
}
}
}
