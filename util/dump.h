#pragma once

#include "../meter/id.h"
#include <map>
#include <string>
#include <vector>

template <typename OStream>
inline OStream& dump_tags(OStream& os, const atlas::meter::Tags& tags) {
  using atlas::util::to_string;

  bool first = true;
  os << '[';
  for (const auto& tag : tags) {
    if (first) {
      first = false;
    } else {
      os << ", ";
    }
    os << to_string(tag.first) << "->" << to_string(tag.second);
  }
  os << ']';
  return os;
}

template <typename OStream, typename T>
inline void dump_vector(OStream& os, const std::vector<T>& vs) {
  bool first = true;
  os << "[";
  for (const auto& v : vs) {
    if (first) {
      first = false;
    } else {
      os << ",";
    }
    os << v;
  }
  os << "]";
}
