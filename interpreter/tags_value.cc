#include "tags_value.h"

namespace atlas {
namespace interpreter {

const OptionalString kNone{nullptr};
const util::StrRef kNameRef = util::intern_str("name");

std::ostream& operator<<(std::ostream& os, const TagsValuePair& tagsValuePair) {
  tagsValuePair.dump(os);
  return os;
}

std::ostream& operator<<(std::ostream& os, const std::shared_ptr<TagsValuePair>& tagsValuePair) {
  tagsValuePair->dump(os);
  return os;
}

std::unique_ptr<TagsValuePair> TagsValuePair::from(const meter::Measurement& measurement, const meter::Tags* common_tags) noexcept {
  return std::make_unique<IdTagsValuePair>(measurement.id, common_tags, measurement.value);
}

std::unique_ptr<TagsValuePair> TagsValuePair::of(meter::Tags&& tags, double value) noexcept {
  return std::make_unique<SimpleTagsValuePair>(std::move(tags), value);
}

}  // namespace interpreter
}  // namespace atlas


