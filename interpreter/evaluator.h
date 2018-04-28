
#pragma once

#include "tags_value.h"
#include "interpreter.h"

namespace atlas {
namespace interpreter {
class Evaluator {
 public:
  Evaluator() noexcept;
  TagsValuePairs eval(const std::string& expression,
                      const TagsValuePairs& measurements) const noexcept;
  std::shared_ptr<Query> get_query(const std::string& expression) const
      noexcept;

 private:
  Interpreter interpreter_;
};

}  // namespace interpreter
}  // namespace atlas
