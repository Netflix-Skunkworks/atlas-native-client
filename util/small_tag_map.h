#include "../meter/tag.h"
#include "logger.h"

#pragma once

namespace atlas {
namespace util {

namespace detail {
static const size_t constexpr prime_list[] = {3lu, 7lu, 17lu, 29lu, 37lu};
static const util::StrRef kNotFound = util::intern_str("");

struct prime_number_hash_policy {
  static size_t mod3(size_t hash) { return hash % 3lu; }
  static size_t mod7(size_t hash) { return hash % 7lu; }
  static size_t mod17(size_t hash) { return hash % 17lu; }
  static size_t mod29(size_t hash) { return hash % 29lu; }
  static size_t mod37(size_t hash) { return hash % 37lu; }

  size_t index_for_hash(size_t hash) const {
    static constexpr size_t (*const mod_functions[])(size_t) = {
        &mod3, &mod7, &mod17, &mod29, &mod37};
    return mod_functions[prime_index](hash);
  }

  size_t next_size_over(size_t* size) const {
    auto n = *size;
    auto idx = prime_index + 1;
    while (prime_list[idx] < n) {
      ++idx;
    }
    *size = prime_list[idx];
    return idx;
  }

  void commit(size_t new_prime_index) { prime_index = new_prime_index; }

  template <typename T>
  size_t pos_for_key(T t) const {
    return index_for_hash(std::hash<T>()(t));
  }

  size_t num_buckets() const { return prime_list[prime_index]; }

  size_t prime_index = 0;
};

static constexpr size_t kMaxTags = 32u;
}  // namespace detail

class SmallTagMap : private detail::prime_number_hash_policy {
  using value_type = std::pair<util::StrRef, util::StrRef>;

 public:
  SmallTagMap() noexcept { init(); }

  SmallTagMap(const SmallTagMap& other) noexcept {
    prime_index = other.prime_index;
    actual_size_ = other.actual_size_;
    auto N = num_buckets();
    entries_.reset(new value_type[N]);
    for (auto i = 0u; i < N; ++i) {
      entries_[i] = other.entries_[i];
    }
  }

  SmallTagMap(SmallTagMap&& other) noexcept {
    prime_index = other.prime_index;
    entries_ = std::move(other.entries_);
    actual_size_ = other.actual_size_;
  }

  SmallTagMap(std::initializer_list<meter::Tag> vs) {
    init();
    for (const auto& tag : vs) {
      add(tag.key, tag.value);
    }
  }

  void add(const char* k, const char* v) noexcept {
    add(util::intern_str(k), util::intern_str(v));
  }

  void add(util::StrRef k_ref, util::StrRef v_ref) noexcept {
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
      if (entries_[i].first.get() != nullptr) {
        throw std::runtime_error("position has already been filled");
      }
      entries_[i].first = k_ref;
      entries_[i].second = v_ref;
      ++actual_size_;
    }
  }

  void add_all(const SmallTagMap& other) {
    auto other_buckets = other.num_buckets();
    for (auto i = 0u; i < other_buckets; ++i) {
      const auto& ki = other.entries_[i].first;
      if (ki.get() != nullptr) {
        add(ki, other.entries_[i].second);
      }
    }
  }

  util::StrRef at(util::StrRef key) const noexcept {
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

  bool has(util::StrRef key) const noexcept {
    return at(key).get() != detail::kNotFound.get();
  }

  size_t size() const noexcept { return actual_size_; }

  size_t hash() const noexcept {
    auto res = 0u;
    auto N = num_buckets();
    for (auto i = 0u; i < N; ++i) {
      const auto& entry = entries_[i];
      if (entry.first.get() != nullptr) {
        res += (std::hash<util::StrRef>()(entry.first) << 1) ^
               std::hash<util::StrRef>()(entry.second);
      }
    }
    return res;
  }

  bool operator==(const SmallTagMap& other) const noexcept {
    if (other.size() != size()) return false;

    const auto N = num_buckets();
    for (auto i = 0u; i < N; ++i) {
      const auto& entry = entries_[i];
      if (entry.first.get() != nullptr) {
        bool eq = entry.second.get() == other.at(entry.first).get();
        if (!eq) return false;
      }
    }
    return true;
  }

  template <typename ValueType>
  struct templated_iterator {
    templated_iterator(const value_type* entries, size_t index,
                       size_t num_buckets)
        : current_idx{index}, entries_(entries), num_buckets_(num_buckets) {}

    friend bool operator==(const templated_iterator& lhs,
                           const templated_iterator& rhs) {
      return lhs.current_idx == rhs.current_idx;
    }

    friend bool operator!=(const templated_iterator& lhs,
                           const templated_iterator& rhs) {
      return lhs.current_idx != rhs.current_idx;
    }

    ValueType& operator*() const { return entries_[current_idx]; }

    templated_iterator& operator++() {
      do {
        ++current_idx;
      } while (current_idx < num_buckets_ && is_empty(current_idx));
      return *this;
    }

   private:
    size_t current_idx;
    const value_type* entries_;
    size_t num_buckets_;

    bool is_empty(size_t idx) { return entries_[idx].first.get() == nullptr; }
  };

  using const_iterator = templated_iterator<const value_type>;

  const_iterator begin() const noexcept {
    auto idx = 0u;
    auto N = num_buckets();
    while (idx < N) {
      if (entries_[idx].first.get() != nullptr) {
        break;
      }
      ++idx;
    }

    return const_iterator(entries_.get(), idx, N);
  }

  const_iterator end() const noexcept {
    size_t N = num_buckets();
    return const_iterator(entries_.get(), N, N);
  }

 private:
  std::unique_ptr<value_type[]> entries_{};
  size_t actual_size_ = 0u;

  void init() {
    size_t buckets = 0;
    auto idx = next_size_over(&buckets);
    commit(idx);
    entries_.reset(new value_type[buckets]);
  }

  void resize(size_t new_desired_size) noexcept {
    auto prev_entries = std::move(entries_);
    auto prev_buckets = num_buckets();

    auto new_bucket_count = new_desired_size;
    auto new_idx = next_size_over(&new_bucket_count);
    entries_.reset(new value_type[new_bucket_count]);
    commit(new_idx);

    actual_size_ = 0;
    for (auto i = 0u; i < prev_buckets; ++i) {
      const auto& entry = prev_entries[i];
      if (entry.first.get() != nullptr) {
        add(entry.first, entry.second);
      }
    }
  }
};

}  // namespace util
}  // namespace atlas
