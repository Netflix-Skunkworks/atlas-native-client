#pragma once

#include "intern.h"
#include "xxhash.h"
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <unordered_map>

namespace atlas {
namespace util {

struct CStrHasher {
  size_t operator()(const char* str) const {
    return XXH64(str, std::strlen(str), 42);
  }
};

struct CStrComparer {
  bool operator()(const char* s1, const char* s2) const {
    return std::strcmp(s1, s2) == 0;
  }
};

class StringPool {
 public:
  StringPool() noexcept {}
  ~StringPool() noexcept;
  StringPool(const StringPool& pool) = delete;
  StringPool& operator=(const StringPool& pool) = delete;

  const StrRef& intern(const char* string) noexcept;

  size_t pool_size() const noexcept { return table.size(); }

  size_t alloc_size() const noexcept { return alloc_size_; }

 private:
  std::unordered_map<const char*, StrRef, CStrHasher, CStrComparer> table;
  size_t alloc_size_ = 0;

  std::mutex mutex;
};

StringPool& the_str_pool() noexcept;
}
}
