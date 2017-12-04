#include "strings.h"
#include "environment.h"
#include "logger.h"
#include <sstream>

#include <pcre.h>

namespace atlas {
namespace util {

#include "valid_chars.inc"

static pcre* get_var_pattern() {
  const char* error;
  int error_offset;

  return pcre_compile(
      R"(\$(\w+)|\$\{([^\}]+)\})", 0, &error, &error_offset, nullptr);
}

static constexpr size_t kOffsetsMax = 30;

std::string ExpandVars(
    const std::string& raw,
    const std::function<std::string(const std::string&)>& expander) noexcept {
  static pcre* var_pattern = get_var_pattern();
  int offsets[kOffsetsMax];
  const char* s = raw.c_str();
  const char* p = s;
  auto pLen = static_cast<int>(raw.length());
  auto dest_index = 0;
  auto source_index = 0;
  char buffer[65536];
  while (true) {
    pLen -= source_index;
    if (pLen == 0) {
      break;
    }
    p += source_index;
    auto rc =
        pcre_exec(var_pattern, nullptr, p, pLen, 0, 0, offsets, kOffsetsMax);
    if (rc < 0) {
      // copy the rest of the string
      if (dest_index + pLen < static_cast<int>(sizeof buffer)) {
        memcpy(buffer + dest_index, p, static_cast<size_t>(pLen));
        dest_index += pLen;
      }
      break;
    }
    if (dest_index + offsets[0] >= static_cast<int>(sizeof buffer)) {
      break;
    }
    memcpy(buffer + dest_index, p, static_cast<size_t>(offsets[0]));
    dest_index += offsets[0];
    int start_offset = offsets[2] > 0 ? 2 : 4;
    std::string to_replace{
        p + offsets[start_offset],
        static_cast<size_t>(offsets[start_offset + 1] - offsets[start_offset])};
    std::string replaced = expander(to_replace);
    if (dest_index + replaced.length() >= sizeof buffer) {
      break;
    }
    memcpy(buffer + dest_index, replaced.c_str(), replaced.length());
    dest_index += replaced.length();
    source_index = offsets[1];
  }
  return std::string{buffer, static_cast<size_t>(dest_index)};
}

std::string ExpandEnvVars(const std::string& raw) noexcept {
  auto expander = [](const std::string& var) {
    const auto& replacement = safe_getenv(var.c_str());
    Logger()->trace("Expanding {}={}", var, replacement);
    return replacement;
  };
  return ExpandVars(raw, expander);
}

static StrRef ToValidTable(const std::array<bool, 128>& table,
                           StrRef atlas_str_ref) noexcept {
  std::string ret;
  std::string atlas_str = atlas_str_ref.get();
  ret.resize(atlas_str.size());
  const auto n = atlas_str.length();
  for (size_t i = 0; i < n; ++i) {
    auto ch = atlas_str[i];
    auto valid = (static_cast<size_t>(ch) >= table.size()) ? false : table[ch];
    ret[i] = valid ? ch : '_';
  }
  return intern_str(ret);
}

StrRef ToValidCharset(StrRef atlas_str_ref) noexcept {
  return ToValidTable(kCharsAllowed, atlas_str_ref);
}

std::string join_path(const std::string& dir, const char* file_name) noexcept {
  std::ostringstream os;
  if (dir.empty()) {
    os << "./";
  } else {
    os << dir;
    if (dir[dir.length() - 1] != '/') {
      os << "/";
    }
  }
  os << file_name;
  return os.str();
}

auto kAsgRef = intern_str("nf.asg");
auto kClusterRef = intern_str("nf.cluster");

StrRef EncodeValueForKey(StrRef value, StrRef key) noexcept {
  auto isGroup = kAsgRef == key || kClusterRef == key;
  const auto& table = isGroup ? kGroupCharsAllowed : kCharsAllowed;
  return ToValidTable(table, value);
}

bool IStartsWith(const std::string& s, const std::string& prefix) noexcept {
  const char* a = s.c_str();
  const char* b = prefix.c_str();
  for (size_t i = 0; i < prefix.size(); ++i) {
    auto source_letter = a[i];
    if (source_letter == '\0') {
      return false;
    }
    auto prefix_letter = tolower(b[i]);
    if (tolower(source_letter) != prefix_letter) {
      return false;
    }
  }
  return true;
}

}  // namespace util
}  // namespace atlas
