#pragma once

#include <map>
#include <ostream>
#include "../types.h"

namespace atlas {
namespace meter {

struct Tag {
  std::string key;
  std::string value;
};

using Tags = std::map<std::string, std::string>;

extern const Tags kEmptyTags;

class Id {
 public:
  Id(std::string name, Tags tags = kEmptyTags) noexcept
      : name_(std::move(name)),
        tags_(std::move(tags)),
        hash_(0u) {}

  bool operator==(const Id& rhs) const noexcept;

  const std::string& Name() const noexcept;

  const Tags& GetTags() const noexcept;

  std::unique_ptr<Id> WithTag(const Tag& tag) const;

  bool operator<(const Id& rhs) const noexcept;

  friend std::ostream& operator<<(std::ostream& os, const Id& id);

  friend struct std::hash<Id>;

  friend struct std::hash<std::shared_ptr<Id>>;

 private:
  std::string name_;
  Tags tags_;
  mutable size_t hash_;

  size_t Hash() const noexcept {
    using std::hash;
    using std::string;

    if (hash_ == 0) {
      // compute hash code, and reuse it
      for (const auto& tag : tags_) {
        hash_ += hash<string>()(tag.first) ^ hash<string>()(tag.second);
      }
      hash_ ^= hash<string>()(name_);
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
