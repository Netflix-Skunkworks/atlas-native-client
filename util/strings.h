#pragma once

#include "intern.h"
#include <algorithm>
#include <cctype>
#include <functional>
#include <initializer_list>
#include <string>
#include <string.h>

namespace atlas {
namespace util {

inline std::string first_nonempty(
    std::initializer_list<std::string> list) noexcept {
  using namespace std;
  auto found = find_if(begin(list), end(list),
                       [](const string& elt) { return !elt.empty(); });
  return found != end(list) ? *found : "";
}

/// expand the template string
/// using the function expander
/// @param raw template string
/// @param expander function that gets called with the var name, and returns
///                 the expanded result. If it returns an empty string var
///                 name will be used instead
/// @return A new string with vars expanded
std::string ExpandVars(
    const std::string& raw,
    const std::function<std::string(const std::string&)>& expander) noexcept;

/// expand the template string using environment variables
std::string ExpandEnvVars(const std::string& raw) noexcept;

/// get the sanitized string representation of atlas_str
/// letters, numbers, dots, and underscores are allowed, otherwise we convert it
/// to _
StrRef ToValidCharset(StrRef atlas_str_ref) noexcept;

/// convert value to a representation atlas would allow
/// some keys have a more relaxed restriction, therefore this
/// function needs to know for which key we need to encode
StrRef EncodeValueForKey(StrRef value, StrRef key) noexcept;

std::string join_path(const std::string& dir, const char* file_name) noexcept;

inline bool StartsWith(StrRef ref, const char* prefix) noexcept {
  return strncmp(ref.get(), prefix, strlen(prefix)) == 0;
}

bool IStartsWith(const std::string& s, const std::string& prefix) noexcept;

inline void TrimRight(std::string* s) {
  s->erase(std::find_if(s->rbegin(), s->rend(),
                        std::not1(std::ptr_fun<int, int>(std::isspace)))
               .base(),
           s->end());
}

inline void ToLower(std::string* s) {
  for (size_t i = 0; i < s->size(); ++i) {
    s->at(i) = static_cast<char>(std::tolower(s->at(i)));
  }
}

inline std::string ToLowerCopy(std::string s) {
  ToLower(&s);
  return s;
}

}  // namespace util
}  // namespace atlas
