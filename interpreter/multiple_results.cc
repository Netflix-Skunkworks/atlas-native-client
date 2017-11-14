#include "multiple_results.h"
#include "../meter/id.h"
#include <sstream>

namespace atlas {
namespace interpreter {

const OptionalString kNone{nullptr};

OptionalString MultipleResults::get_value(const TagsValuePair& tagsValuePair,
                                          const util::StrRef k) {
  auto v = tagsValuePair.tags.at(k);
  if (*v.get() != '\0') {
    return OptionalString{v.get()};
  }

  return kNone;
}

std::string MultipleResults::keys_str(StringRefs strings) {
  std::ostringstream os;

  auto first = true;
  for (const auto& s : strings) {
    if (!first) {
      os << ',';
    } else {
      first = false;
    }
    os << s.get();
  }
  return os.str();
}
}  // namespace interpreter
}  // namespace atlas
