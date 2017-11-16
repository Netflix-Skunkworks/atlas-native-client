#include "small_tag_map.h"
#include "logger.h"

namespace atlas {
namespace util {

void SmallTagMap::add(util::StrRef k_ref, util::StrRef v_ref) noexcept {
  const auto pos = pos_for_key(k_ref);
  auto i = pos;
  auto ki = entries_[i].first;
  while (ki.get() != nullptr && ki.get() != k_ref.get()) {
    i = index_for_hash(i + 1);  // avoid expensive mod variable operation
    if (i == pos) {
      if (actual_size_ >= detail::kMaxTags) {
        Logger()->error("cannot add tag({},{}) - Maximum of {} tags reached.",
                        k_ref.get(), v_ref.get(), detail::kMaxTags);
        return;
      }
      resize(actual_size_ + 1);
      add(k_ref, v_ref);
      return;
    }
    ki = entries_[i].first;
  }

  if (ki.get() != nullptr) {
    entries_[i].second = v_ref;
  } else {
    if (entries_[i].first.valid()) {
      throw std::runtime_error("position has already been filled");
    }
    entries_[i].first = k_ref;
    entries_[i].second = v_ref;
    ++actual_size_;
  }
}

util::StrRef SmallTagMap::at(util::StrRef key) const noexcept {
  auto pos = pos_for_key(key);
  auto i = pos;
  auto ki = entries_[i].first;
  while (ki.get() != nullptr && ki.get() != key.get()) {
    i = index_for_hash(i + 1);
    if (i == pos) {
      return detail::kNotFound;
    }
    ki = entries_[i].first;
  }
  return ki.get() == nullptr ? detail::kNotFound : entries_[i].second;
}

size_t SmallTagMap::hash() const noexcept {
  auto res = 0u;
  auto N = num_buckets();
  for (auto i = 0u; i < N; ++i) {
    const auto& entry = entries_[i];
    if (entry.first.valid()) {
      res += (std::hash<util::StrRef>()(entry.first) << 1) ^
             std::hash<util::StrRef>()(entry.second);
    }
  }
  return res;
}

bool SmallTagMap::operator==(const SmallTagMap& other) const noexcept {
  if (other.size() != size()) return false;

  const auto N = num_buckets();
  for (auto i = 0u; i < N; ++i) {
    const auto& entry = entries_[i];
    if (entry.first.valid()) {
      bool eq = entry.second == other.at(entry.first);
      if (!eq) return false;
    }
  }
  return true;
}
void SmallTagMap::resize(size_t new_desired_size) noexcept {
  auto prev_entries = std::move(entries_);
  auto prev_buckets = num_buckets();

  auto new_bucket_count = new_desired_size;
  auto new_idx = next_size_over(&new_bucket_count);
  entries_.reset(new value_type[new_bucket_count]);
  commit(new_idx);

  actual_size_ = 0;
  for (auto i = 0u; i < prev_buckets; ++i) {
    const auto& entry = prev_entries[i];
    if (entry.first.valid()) {
      add(entry.first, entry.second);
    }
  }
}

}  // namespace util
}  // namespace atlas
