#include "id.h"

#include "id_format.h"
#include "statistic.h"
#include <utility>

namespace atlas {
namespace meter {

const Tags kEmptyTags;

const char* Id::Name() const noexcept { return name_.get(); }

const Tags& Id::GetTags() const noexcept { return tags_; }

std::unique_ptr<Id> Id::WithTag(const Tag& tag) const {
  Tags new_tags(tags_);
  new_tags.add(tag);
  return std::make_unique<Id>(name_, new_tags);
}

bool Id::operator==(const Id& rhs) const noexcept {
  return std::tie(name_, tags_) == std::tie(rhs.name_, rhs.tags_);
}

IdPtr WithDefaultTagForId(const IdPtr& id, const Tag& default_tag) {
  bool already_has_key = id->GetTags().has(default_tag.key);
  return already_has_key ? id : id->WithTag(default_tag);
}

const Tag kGaugeDsType = Tag::of("atlas.dstype", "gauge");

IdPtr WithDefaultGaugeTags(const IdPtr& id) {
  return WithDefaultGaugeTags(id, statistic::gauge);
}

IdPtr WithDefaultGaugeTags(const IdPtr& id, const Tag& stat) {
  return WithDefaultTagForId(WithDefaultTagForId(id, stat), kGaugeDsType);
}

std::ostream& operator<<(std::ostream& os, const Id& id) {
  os << "Id(" << id.Name() << ", " << id.GetTags() << ")";
  return os;
}

}  // namespace meter
}  // namespace atlas
