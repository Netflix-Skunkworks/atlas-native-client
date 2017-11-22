#pragma once

#include <cstdint>
#include <string>
#include <cstring>

namespace atlas {
namespace util {

class StringPool;

class StrRef {
 public:
  StrRef() = default;
  bool operator==(const StrRef& rhs) const { return data == rhs.data; }
  bool operator!=(const StrRef& rhs) const { return data != rhs.data; }
  const char* get() const { return data; }
  bool empty() const { return data == nullptr; }
  bool valid() const { return data != nullptr; }
  size_t length() const { return std::strlen(data); }

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
