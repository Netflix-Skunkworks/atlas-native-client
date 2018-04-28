#pragma once

#include <memory>
#include <string>
#include <vector>

namespace atlas {

// maybe switch to fbvector as well
using Strings = std::vector<std::string>;
}  // namespace atlas

namespace std {
template <typename T, typename... Args>
unique_ptr<T> make_unique(Args&&... args) {
  return unique_ptr<T>(new T(forward<Args>(args)...));
}
}  // namespace std
