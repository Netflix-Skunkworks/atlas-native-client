#include "keep_drop_tags.h"
#include "../meter/id.h"
#include "../meter/measurement.h"
#include <algorithm>
#include <ska/flat_hash_map.hpp>

namespace atlas {
namespace interpreter {

using meter::Measurements;
using util::intern_str;
using util::StrRef;

KeepOrDropTags::KeepOrDropTags(const List& keys,
                               std::shared_ptr<ValueExpression> expr, bool keep)
    : keys_(keys.ToStrings()), expr_(std::move(expr)), keep_(keep) {
  auto name = name_ref();
  if (keep) {
    // make sure we keep name, it's required
    if (std::find(keys_->begin(), keys_->end(), name) == keys_->end()) {
      keys_->push_back(name);
    }
  }
}

using StringRefs = std::vector<StrRef>;
static StringRefs drop_keys(const meter::Tags& tags, const StringRefs& keys) {
  auto name = name_ref();
  StringRefs res{name};

  // add all tag keys that are not in the set of keys to be dropped
  for (const auto& tag : tags) {
    if (tag.first != name &&
        std::find(keys.begin(), keys.end(), tag.first) == keys.end()) {
      res.push_back(tag.first);
    }
  }
  return res;
}

TagsValuePairs KeepOrDropTags::Apply(const TagsValuePairs& measurements) {
  // group metrics by keys
  ska::flat_hash_map<meter::Tags, TagsValuePairs> grouped;
  for (auto& valuePair : measurements) {
    auto should_keep = true;
    meter::Tags group_by_vals;

    const auto& keys =
        keep_ ? *keys_ : drop_keys(valuePair->all_tags(), *keys_);

    for (auto& key : keys) {
      auto value = get_value(*valuePair, key);
      if (value) {
        group_by_vals.add(key, intern_str(value.get()));
      } else {
        should_keep = false;
        break;
      }
    }
    if (should_keep) {
      grouped[group_by_vals].push_back(valuePair);
    }
  }

  TagsValuePairs results{};
  for (auto& entry : grouped) {
    auto tags = entry.first;
    const auto& measurements_for_tags = entry.second;
    const auto& expr_results = expr_->Apply(measurements_for_tags);
    auto v = expr_results->value();
    if (std::isnan(v)) {
      continue;
    }
    results.emplace_back(TagsValuePair::of(std::move(tags), v));
  }

  return results;
}

static const char* kKeep = "Keep";
static const char* kDrop = "Drop";
std::ostream& KeepOrDropTags::Dump(std::ostream& os) const {
  const char* keep_or_drop = keep_ ? kKeep : kDrop;
  os << keep_or_drop << "Tags(" << keys_str(*keys_) << "," << *expr_ << ")";
  return os;
}
}  // namespace interpreter
}  // namespace atlas
