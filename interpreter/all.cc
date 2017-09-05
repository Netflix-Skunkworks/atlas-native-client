#include "all.h"

namespace atlas {
namespace interpreter {

All::All(std::shared_ptr<ValueExpression> expr) : expr_(std::move(expr)) {}

TagsValuePairs All::Apply(const TagsValuePairs& measurements) {
  return measurements;
}

std::ostream& All::Dump(std::ostream& os) const {
  os << "All(" << *expr_ << ")";
  return os;
}
}  // namespace interpreter
}  // namespace atlas
