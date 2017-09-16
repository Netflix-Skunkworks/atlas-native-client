#pragma once

#include <cstdint>
#include <string>

namespace atlas {
namespace util {

class StrRef {
 public:
  StrRef() = default;
  bool operator==(const StrRef& rhs) const { return data == rhs.data; }
  const char* data = nullptr;
};

const StrRef& intern_str(const char* string);
const StrRef& intern_str(const std::string& string);
const char* to_string(const StrRef& ref);
}
}

namespace std {
template <>
struct hash<atlas::util::StrRef> {
  size_t operator()(const atlas::util::StrRef& ref) const {
    return reinterpret_cast<size_t>(ref.data);
  }
};
}
