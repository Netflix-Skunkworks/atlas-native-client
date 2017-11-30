#pragma once

class OptionalString {
 public:
  explicit OptionalString(const char* string) noexcept
      : has_value(string != nullptr), s(string != nullptr ? string : "") {}

  explicit OptionalString(const std::string& string)
      : has_value{true}, s{string} {}

  operator bool() const { return has_value; }

  operator const std::string&() const { return s; };

  bool operator==(const std::string& other) const {
    return has_value && s == other;
  }

  bool operator<(const std::string& other) const {
    return has_value && s < other;
  }

  bool operator<=(const std::string& other) const {
    return has_value && s <= other;
  }

  bool operator>(const std::string& other) const {
    return has_value && s > other;
  }

  bool operator>=(const std::string& other) const {
    return has_value && s >= other;
  }

  const std::string* operator->() const { return &s; }

  const std::string& operator*() const { return s; }

  const std::string& get() const { return s; }

 private:
  bool has_value;
  std::string s;
};
