#pragma once

#include "../meter/id.h"
#include <map>
#include <string>
#include <vector>

template <typename OStream>
inline OStream& dump_tags(OStream& os, const atlas::meter::Tags& tags) {
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
