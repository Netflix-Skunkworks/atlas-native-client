#pragma once

#include "../meter/measurement.h"
#include <algorithm>
#include <iterator>
#include <fmt/format.h>
#include <sstream>

inline std::string string_join(const std::vector<std::string>& vec,
                               const char* separator) {
  switch (vec.size()) {
    case 0:
      return "";
    case 1:
      return vec.front();
    default:
      std::ostringstream os;
      std::copy(vec.begin(), vec.end() - 1,
                std::ostream_iterator<std::string>(os, separator));
      os << vec.back();
      return os.str();
  }
}

inline std::string tags_to_string(const atlas::meter::Tags& tags) {
  std::vector<std::string> a_vec;
  for (const auto& tag : tags) {
    a_vec.emplace_back(fmt::format("{}={}", tag.first.get(), tag.second.get()));
  }
  std::sort(a_vec.begin(), a_vec.end());
  return string_join(a_vec, ",");
}

inline bool operator<(const atlas::meter::Tags& a,
                      const atlas::meter::Tags& b) {
  return tags_to_string(a) < tags_to_string(b);
}

inline bool operator<(const atlas::meter::Id& a, const atlas::meter::Id& b) {
  auto name_cmp = strcmp(a.Name(), b.Name());
  if (name_cmp < 0) {
    return true;
  } else if (name_cmp > 0) {
    return false;
  }

  return a.GetTags() < b.GetTags();
}
