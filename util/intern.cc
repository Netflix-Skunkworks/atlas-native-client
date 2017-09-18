#include "intern.h"
#include "xxhash.h"
#include <cassert>
#include <cstring>
#include <mutex>
#include <unordered_map>

namespace atlas {
namespace util {

struct Hasher {
  size_t operator()(const char* str) const {
    return XXH64(str, strlen(str), 42);
  }
};

struct Comparer {
  bool operator()(const char* s1, const char* s2) const {
    return std::strcmp(s1, s2) == 0;
  }
};

class StringPool {
 public:
  StringPool() noexcept {}
  ~StringPool() noexcept = default;
  StringPool(const StringPool& pool) = delete;
  StringPool& operator=(const StringPool& pool) = delete;

  const StrRef& intern(const char* string) {
    std::lock_guard<std::mutex> guard(mutex);
    auto it = table.find(string);
    if (it != table.end()) {
      return it->second;
    }
    const auto* copy = strdup(string);
    StrRef ref;
    ref.data = copy;
    table.insert(std::make_pair(copy, ref));
    return table.at(copy);
  }

 private:
  std::unordered_map<const char*, StrRef, Hasher, Comparer> table;

  std::mutex mutex;
};

static StringPool the_pool;

const StrRef& intern_str(const char* string) {
  const auto& r = the_pool.intern(string);
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
