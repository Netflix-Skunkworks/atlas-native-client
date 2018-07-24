#pragma once

#include "intern.h"

class OptionalString {
 public:
  explicit OptionalString(atlas::util::StrRef ref) noexcept : s_ref(ref) {}

  explicit OptionalString(const char* string) noexcept {
    if (string != nullptr) {
      s_ref = atlas::util::intern_str(string);
    }
  }

  operator bool() const { return s_ref.valid(); }

  operator const char*() const { return s_ref.get(); };

  bool operator==(const std::string& other) const {
    return s_ref.valid() && std::string(s_ref.get()) == other;
  }

  bool operator==(const char* other) const {
    return s_ref.valid() && atlas::util::intern_str(other) == s_ref;
  }

  bool operator==(atlas::util::StrRef other) const { return s_ref == other; }

  bool operator<(atlas::util::StrRef other) const {
    if (s_ref.valid() && other.valid()) {
      return strcmp(s_ref.get(), other.get()) < 0;
    }
    return false;
  }

  bool operator<=(atlas::util::StrRef other) const {
    if (s_ref.valid() && other.valid()) {
      return strcmp(s_ref.get(), other.get()) <= 0;
    }
    return false;
  }

  bool operator>(atlas::util::StrRef other) const {
    if (s_ref.valid() && other.valid()) {
      return strcmp(s_ref.get(), other.get()) > 0;
    }
    return false;
  }

  bool operator>=(atlas::util::StrRef other) const {
    if (s_ref.valid() && other.valid()) {
      return strcmp(s_ref.get(), other.get()) >= 0;
    }
    return false;
  }

  const char* get() const { return s_ref.get(); }

  size_t length() const { return s_ref.length(); }

 private:
  atlas::util::StrRef s_ref;
};
