#include "../meter/tag.h"
#include <memory>

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

  SmallTagMap(std::initializer_list<std::pair<const char*, const char*>> vs) {
    init();
    for (const auto& tag : vs) {
      add(tag.first, tag.second);
    }
  }

  void add(const char* k, const char* v) noexcept {
    add(util::intern_str(k), util::intern_str(v));
  }

  void add(util::StrRef k_ref, util::StrRef v_ref) noexcept;

  void add_all(const SmallTagMap& other) {
    auto other_buckets = other.num_buckets();
    for (auto i = 0u; i < other_buckets; ++i) {
      const auto& ki = other.entries_[i].first;
      if (ki.get() != nullptr) {
        add(ki, other.entries_[i].second);
      }
    }
  }

  util::StrRef at(util::StrRef key) const noexcept;

  bool has(util::StrRef key) const noexcept {
    return at(key) != detail::kNotFound;
  }

  size_t size() const noexcept { return actual_size_; }

  size_t hash() const noexcept;

  bool operator==(const SmallTagMap& other) const noexcept;

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

    bool is_empty(size_t idx) { return entries_[idx].first.is_null(); }
  };

  using const_iterator = templated_iterator<const value_type>;

  const_iterator begin() const noexcept {
    auto idx = 0u;
    auto N = num_buckets();
    while (idx < N) {
      if (entries_[idx].first.valid()) {
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

  void resize(size_t new_desired_size) noexcept;
};

}  // namespace util
}  // namespace atlas
