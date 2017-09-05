#pragma once

#include "../meter/measurement.h"
#include <cstdlib>
#include <cmath>

namespace atlas {
namespace interpreter {

struct TagsValuePair {
  meter::Tags tags;
  double value;
  static TagsValuePair from(const meter::Measurement& measurement,
                            const meter::Tags& common_tags) noexcept;
};

inline bool operator==(const TagsValuePair& lhs, const TagsValuePair& rhs) {
  return lhs.tags == rhs.tags && std::abs(lhs.value - rhs.value) < 1e-6;
}
std::ostream& operator<<(std::ostream& os, const TagsValuePair& tagsValuePair);

using TagsValuePairs = std::vector<TagsValuePair>;

}  // namespace interpreter
}  // namespace atlas
