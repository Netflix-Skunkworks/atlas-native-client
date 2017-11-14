#pragma once

#include <cstdint>
#include <string>

namespace atlas {
namespace util {

class StringPool;

class StrRef {
 public:
  StrRef() = default;
  bool operator==(const StrRef& rhs) const { return data == rhs.data; }
  const char* get() const { return data; }

 private:
  const char* data = nullptr;
  friend std::hash<StrRef>;
  friend StringPool;
};

const StrRef& intern_str(const char* string);
const StrRef& intern_str(const std::string& string);
}
}

namespace std {
template <>
struct hash<atlas::util::StrRef> {
  size_t operator()(const atlas::util::StrRef& ref) const {
    auto pointer_value = reinterpret_cast<uint64_t>(ref.data);
    // get rid of lower bits since the pointer will usually be 16-byte aligned
    return pointer_value >> 4;
  }
};
}
