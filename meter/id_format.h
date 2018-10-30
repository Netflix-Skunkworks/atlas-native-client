#pragma once

#include "id.h"
#include <vector>
#include <fmt/ostream.h>

namespace atlas {
namespace meter {

inline std::ostream& operator<<(std::ostream& os, const Tags& tags) {
  bool first = true;
  os << '[';
  for (const auto& tag : tags) {
    if (first) {
      first = false;
    } else {
      os << ", ";
    }
    os << tag.first.get() << "->" << tag.second.get();
  }
  os << ']';
  return os;
}

}  // namespace meter
}  // namespace atlas

namespace std {

inline ostream& operator<<(ostream& os, const vector<string>& vs) {
  os << '[';
  auto it = vs.begin();
  const auto& end = vs.end();
  if (it != end) {
    os << *it;
    ++it;
  }
  for (; it != end; ++it) {
    os << ',';
    os << *it;
  }
  os << ']';
  return os;
}

}  // namespace std
