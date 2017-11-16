#include "all.h"

namespace atlas {
namespace interpreter {

All::All(std::shared_ptr<ValueExpression> expr) : expr_(std::move(expr)) {}

std::shared_ptr<TagsValuePairs> All::Apply(std::shared_ptr<TagsValuePairs> measurements) {
  return measurements;
}

std::ostream& All::Dump(std::ostream& os) const {
  os << "All(" << *expr_ << ")";
  return os;
}
}  // namespace interpreter
}  // namespace atlas
