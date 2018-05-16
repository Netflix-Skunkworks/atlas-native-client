#include "tags_value.h"
#include "../meter/id_format.h"

namespace atlas {
namespace interpreter {

const OptionalString kNone{nullptr};

util::StrRef name_ref() {
  static util::StrRef name = util::intern_str("name");
  return name;
}

void IdTagsValuePair::dump(std::ostream& os) const noexcept {
  os << "IdTagsValuePair(id=" << *id_ << ",common_tags=" << *common_tags_
     << ",value=" << value_ << ")";
}

void SimpleTagsValuePair::dump(std::ostream& os) const noexcept {
  os << "SimpleTagsValuePair(tags=" << tags_ << ",value=" << value_ << ")";
}

std::ostream& operator<<(std::ostream& os, const TagsValuePair& tagsValuePair) {
  tagsValuePair.dump(os);
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const std::shared_ptr<TagsValuePair>& tagsValuePair) {
  tagsValuePair->dump(os);
  return os;
}

std::unique_ptr<TagsValuePair> TagsValuePair::from(
    const meter::Measurement& measurement,
    const meter::Tags* common_tags) noexcept {
  return std::make_unique<IdTagsValuePair>(measurement.id, common_tags,
                                           measurement.value);
}

std::unique_ptr<TagsValuePair> TagsValuePair::of(meter::Tags&& tags,
                                                 double value) noexcept {
  return std::make_unique<SimpleTagsValuePair>(std::move(tags), value);
}

}  // namespace interpreter
}  // namespace atlas
