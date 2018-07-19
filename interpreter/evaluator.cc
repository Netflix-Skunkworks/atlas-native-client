#include "evaluator.h"
#include "group_by.h"

namespace atlas {
namespace interpreter {

Evaluator::Evaluator() noexcept
    : interpreter_(std::make_unique<interpreter::ClientVocabulary>()) {}

TagsValuePairs Evaluator::eval(const std::string& expression,
                               const TagsValuePairs& measurements) const
    noexcept {
  auto results = TagsValuePairs();

  if (measurements.empty()) {
    return results;
  }

  interpreter::Context context;
  interpreter_.Execute(&context, expression);

  // for each expression on the stack
  while (context.StackSize() > 0) {
    auto by = interpreter::GetMultipleResultsExpr(context.PopExpression());
    if (by) {
      auto expression_result = by->Apply(measurements);
      std::move(expression_result.begin(), expression_result.end(),
                std::back_inserter(results));
    }
  }

  return results;
}

std::shared_ptr<Query> Evaluator::get_query(const std::string& expression) const
    noexcept {
  return interpreter_.GetQuery(expression);
}

}  // namespace interpreter
}  // namespace atlas
