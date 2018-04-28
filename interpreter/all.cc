#include "all.h"
#include "../meter/id_format.h"
#include "../util/logger.h"

namespace atlas {
namespace interpreter {

All::All(std::shared_ptr<Query> query) : query_(std::move(query)) {}

TagsValuePairs All::Apply(const TagsValuePairs& measurements) {
  if (query_->IsTrue()) {
    // fast path
    return measurements;
  }

  TagsValuePairs result;
  auto q = query_.get();
  std::copy_if(measurements.begin(), measurements.end(),
               std::back_inserter(result),
               [q](const std::shared_ptr<TagsValuePair>& m) {
                 return !std::isnan(m->value()) && q->Matches(*m);
               });
  return result;
}

std::ostream& All::Dump(std::ostream& os) const {
  os << "All(" << *query_ << ")";
  return os;
}
}  // namespace interpreter
}  // namespace atlas
