#pragma once

#include "context.h"
#include "expression.h"

namespace atlas {
namespace interpreter {

class MultipleResults : public Expression {
 public:
  virtual TagsValuePairs Apply(const TagsValuePairs& tagsValuePairs) = 0;
  virtual std::shared_ptr<Query> GetQuery() const noexcept = 0;

 protected:
  static OptionalString get_value(const TagsValuePair& tagsValuePair,
                                  const std::string& k);
  static std::string keys_str(std::vector<std::string>);
};
}
}
