#include "../util/intern.h"

namespace atlas {
namespace meter {

struct Tag {
  util::StrRef key;
  util::StrRef value;

  Tag() noexcept {}
  Tag(util::StrRef k, util::StrRef v) noexcept : key{k}, value{v} {}

  static Tag of(const char* k, const char* v) noexcept {
    return Tag{util::intern_str(k), util::intern_str(v)};
  }
  static Tag of(const std::string& k, const std::string& v) noexcept {
    return Tag{util::intern_str(k), util::intern_str(v)};
  }
};

}  // namespace meter
}  // namespace atlas
