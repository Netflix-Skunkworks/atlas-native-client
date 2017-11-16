#include "string_pool.h"

namespace atlas {
namespace util {

StringPool& the_str_pool() noexcept {
  static auto* the_pool = new StringPool();
  return *the_pool;
}

StringPool::~StringPool() noexcept {
  for (const auto& kv : table) {
    auto copy = static_cast<void*>(const_cast<char*>(kv.first));
    free(copy);
  }
}

const StrRef& StringPool::intern(const char* string) noexcept {
  std::lock_guard<std::mutex> guard(mutex);
  auto it = table.find(string);
  if (it != table.end()) {
    return it->second;
  }
  const auto* copy = strdup(string);
  StrRef ref;
  ref.data = copy;
  table.insert(std::make_pair(copy, ref));
  alloc_size_ += strlen(copy) + 1;  // null terminator
  return table.at(copy);
}

}  // namespace util
}  // namespace atlas
