#include "intern.h"
#include "string_pool.h"

namespace atlas {
namespace util {

const StrRef& intern_str(const char* string) {
  const auto& r = the_str_pool.intern(string);
#ifdef DEBUG
  if (strcmp(string, r.get()) != 0) {
    printf("Unable to intern %s = %s -> %s\n", string, r.data, r.get());
    abort();
  }
#endif
  return r;
}

const StrRef& intern_str(const std::string& string) {
  return intern_str(string.c_str());
}

}  // namespace util
}  // namespace atlas
