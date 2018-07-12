#pragma once

#include "../meter/measurement.h"
#include "../util/optional.h"
#include <cstdlib>
#include <cmath>

namespace atlas {
namespace interpreter {

class TagsValuePair {
 public:
  virtual OptionalString get_value(util::StrRef key) const noexcept = 0;
  virtual double value() const noexcept = 0;
  virtual meter::Tags all_tags() const noexcept = 0;
  virtual ~TagsValuePair() = default;
  virtual void dump(std::ostream& os) const noexcept = 0;

  static std::unique_ptr<TagsValuePair> from(
      const meter::Measurement& measurement,
      const meter::Tags* common_tags) noexcept;
  static std::unique_ptr<TagsValuePair> of(meter::Tags&& tags,
                                           double value) noexcept;
};

extern const OptionalString kNone;
util::StrRef name_ref();

// A TagsValuePair created from a full set of tags and a value
class SimpleTagsValuePair : public TagsValuePair {
 public:
  SimpleTagsValuePair(meter::Tags&& tags, double value)
      : tags_(tags), value_(value) {}

  OptionalString get_value(util::StrRef key) const noexcept override {
    auto v = tags_.at(key);
    if (v.valid()) {
      return OptionalString{v.get()};
    }

    return kNone;
  }

  meter::Tags all_tags() const noexcept override { return tags_; }

  void dump(std::ostream& os) const noexcept override;

  double value() const noexcept override { return value_; }

 private:
  meter::Tags tags_;
  double value_;
};

// A TagsValuePair where tags are provided by an id and common tags
class IdTagsValuePair : public TagsValuePair {
 public:
  IdTagsValuePair(meter::IdPtr id, const meter::Tags* common_tags, double value)
      : id_(std::move(id)), common_tags_(common_tags), value_(value) {}

  OptionalString get_value(util::StrRef key) const noexcept override {
    if (key.get() == name_ref().get()) {
      return OptionalString{id_->Name()};
    }

    const auto& tags = id_->GetTags();
    auto v = tags.at(key);
    if (v.valid()) {
      return OptionalString{v.get()};
    }

    auto cv = common_tags_->at(key);
    if (cv.valid()) {
      return OptionalString{cv.get()};
    }
    return kNone;
  }

  double value() const noexcept override { return value_; }

  meter::Tags all_tags() const noexcept override {
    meter::Tags tags{*common_tags_};
    tags.add_all(id_->GetTags());
    tags.add(name_ref(), id_->NameRef());
    return tags;
  }

  void dump(std::ostream& os) const noexcept override;

 private:
  meter::IdPtr id_;
  const meter::Tags* common_tags_;
  double value_;
};

std::ostream& operator<<(std::ostream& os, const TagsValuePair& tagsValuePair);

std::ostream& operator<<(std::ostream& os,
                         const std::shared_ptr<TagsValuePair>& tagsValuePair);

inline bool operator==(const TagsValuePair& lhs, const TagsValuePair& rhs) {
  return lhs.all_tags() == rhs.all_tags() &&
         std::abs(lhs.value() - rhs.value()) < 1e-6;
}

inline bool operator==(const std::shared_ptr<TagsValuePair>& lhs,
                       const std::shared_ptr<TagsValuePair>& rhs) {
  return *lhs == *rhs;
}

using TagsValuePairs = std::vector<std::shared_ptr<TagsValuePair>>;

}  // namespace interpreter
}  // namespace atlas
