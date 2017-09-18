#include "multiple_results.h"
#include "../meter/id.h"
#include <sstream>

namespace atlas {
namespace interpreter {

const OptionalString kNone{nullptr};

OptionalString MultipleResults::get_value(const TagsValuePair& tagsValuePair,
                                          const util::StrRef k) {
  auto it = tagsValuePair.tags.find(k);
  if (it != tagsValuePair.tags.end()) {
    return OptionalString{(*it).second.get()};
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
