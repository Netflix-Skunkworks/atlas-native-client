#pragma once

#include <map>
#include <ostream>
#include <unordered_map>
#include "../types.h"
#include "../util/intern.h"
#include "../util/small_tag_map.h"

namespace atlas {
namespace meter {

class Tags {
  using K = util::StrRef;
  using table_t = util::SmallTagMap;
  using value_t = util::StrRef;
  table_t entries_;

 public:
  Tags() = default;

  Tags(std::initializer_list<Tag> vs) : entries_(vs) {}

  Tags(std::initializer_list<std::pair<const char*, const char*>> vs) {
    for (const auto& pair : vs) {
      add(pair.first, pair.second);
    }
  }

  void add(const Tag& tag) { entries_.add(tag.key, tag.value); }

  void add(K k, K v) { entries_.add(k, v); }

  void add(const char* k, const char* v) { entries_.add(k, v); }

  size_t hash() const { return entries_.hash(); }

  void add_all(const Tags& source) { entries_.add_all(source.entries_); }

  bool operator==(const Tags& that) const { return that.entries_ == entries_; }

  bool has(K key) const { return entries_.has(key); }

  K at(K key) const { return entries_.at(key); }

  size_t size() const { return entries_.size(); }

  table_t::const_iterator begin() const { return entries_.begin(); }

  table_t::const_iterator end() const { return entries_.end(); }
};

extern const Tags kEmptyTags;

class Id {
 public:
  Id(util::StrRef name, Tags tags) noexcept
      : name_(name), tags_(std::move(tags)), hash_(0u) {}
  Id(const std::string& name, Tags tags) noexcept
      : name_(util::intern_str(name)), tags_(std::move(tags)), hash_(0u) {}

  bool operator==(const Id& rhs) const noexcept;

  const char* Name() const noexcept;

  util::StrRef NameRef() const noexcept { return name_; };

  const Tags& GetTags() const noexcept;

  std::unique_ptr<Id> WithTag(const Tag& tag) const;

  friend std::ostream& operator<<(std::ostream& os, const Id& id);

  friend struct std::hash<Id>;

  friend struct std::hash<std::shared_ptr<Id>>;

 private:
  util::StrRef name_;
  Tags tags_;
  mutable size_t hash_;

  size_t Hash() const noexcept {
    if (hash_ == 0) {
      // compute hash code, and reuse it
      hash_ = tags_.hash() ^ std::hash<atlas::util::StrRef>()(name_);
    }

    return hash_;
  }
};

using IdPtr = std::shared_ptr<Id>;

IdPtr WithDefaultTagForId(IdPtr id, const Tag& default_tag);

IdPtr WithDefaultGaugeTags(IdPtr id);

IdPtr WithDefaultGaugeTags(IdPtr id, const Tag& stat);

}  // namespace meter
}  // namespace atlas

namespace std {
template <>
struct hash<atlas::meter::Id> {
  size_t operator()(const atlas::meter::Id& id) const { return id.Hash(); }
};

template <>
struct hash<atlas::meter::Tags> {
  size_t operator()(const atlas::meter::Tags& tags) const {
    return tags.hash();
  }
};

template <>
struct hash<shared_ptr<atlas::meter::Id>> {
  size_t operator()(const shared_ptr<atlas::meter::Id>& id) const {
    return id->Hash();
  }
};

template <>
struct equal_to<shared_ptr<atlas::meter::Id>> {
  bool operator()(const shared_ptr<atlas::meter::Id>& lhs,
                  const shared_ptr<atlas::meter::Id>& rhs) const {
    return *lhs == *rhs;
  }
};
}  // namespace std
