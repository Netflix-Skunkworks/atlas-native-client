#pragma once

#include <map>
#include <ostream>
#include <unordered_map>
#include "../types.h"
#include "../util/intern.h"

namespace atlas {
namespace meter {

struct Tag {
  util::StrRef key;
  util::StrRef value;

  Tag(util::StrRef k, util::StrRef v) noexcept : key{k}, value{v} {}

  static Tag of(const std::string& k, const char* v) {
    return Tag{util::intern_str(k), util::intern_str(v)};
  }
  static Tag of(const std::string& k, const std::string& v) {
    return Tag{util::intern_str(k), util::intern_str(v)};
  }
  static Tag of(const std::string& k, int64_t v) {
    std::string s = std::to_string(v);
    return Tag{util::intern_str(k), util::intern_str(s)};
  }
};

class Tags {
  using K = util::StrRef;
  using table_t = std::unordered_map<K, K>;
  using value_t = table_t::value_type;
  table_t entries_;

  static inline table_t to_str_refs(
      std::initializer_list<std::pair<const char*, const char*>>(vs)) {
    table_t res;
    for (const auto& pair : vs) {
      res.insert(std::make_pair(util::intern_str(pair.first),
                                util::intern_str(pair.second)));
    }
    return res;
  }

 public:
  Tags() = default;

  Tags(std::initializer_list<value_t> vs) : entries_(vs) {}

  Tags(std::initializer_list<std::pair<const char*, const char*>> vs)
      : entries_(to_str_refs(vs)) {}

  void add(const Tag& tag) { entries_[tag.key] = tag.value; }

  void add(K k, K v) { entries_[k] = v; }

  void add(const char* k, const char* v) {
    entries_[util::intern_str(k)] = util::intern_str(v);
  }

  size_t hash() const {
    size_t res = 0;
    for (const auto& tag : entries_) {
      res += std::hash<K>()(tag.first) ^ std::hash<K>()(tag.second);
    }
    return res;
  }

  void add_all(const Tags& source) {
    entries_.insert(source.entries_.begin(), source.entries_.end());
  }

  bool operator==(const Tags& that) const { return that.entries_ == entries_; }

  table_t::const_iterator begin() const { return entries_.begin(); }

  table_t::const_iterator end() const { return entries_.end(); }

  bool has(K key) const { return entries_.count(key) == 1; }

  table_t::const_iterator find(const K& k) const { return entries_.find(k); }

  K at(K key) const {
    auto it = entries_.find(key);
    if (it != entries_.end()) {
      return it->second;
    }
    throw std::out_of_range(std::string("Unknown key: ") +
                            util::to_string(key));
  }

  size_t size() const { return entries_.size(); }
};

extern const Tags kEmptyTags;

class Id {
 public:
  Id(util::StrRef name, Tags tags) noexcept : name_(name),
                                              tags_(std::move(tags)),
                                              hash_(0u) {}
  Id(const std::string& name, Tags tags) noexcept
      : name_(util::intern_str(name)),
        tags_(std::move(tags)),
        hash_(0u) {}

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
