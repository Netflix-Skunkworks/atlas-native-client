#include "keep_drop_tags.h"
#include "../meter/id.h"
#include "../meter/measurement.h"
#include "../util/dump.h"
#include <algorithm>
#include <unordered_set>
#include <iostream>

namespace atlas {
namespace interpreter {

using meter::Measurements;
using util::StrRef;
using util::intern_str;

const static auto kNameRef = intern_str("name");

KeepOrDropTags::KeepOrDropTags(const List& keys,
                               std::shared_ptr<ValueExpression> expr, bool keep)
    : keys_(keys.ToStrings()), expr_(std::move(expr)), keep_(keep) {
  if (keep) {
    // make sure we keep name, it's required
    if (std::find(keys_->begin(), keys_->end(), kNameRef) == keys_->end()) {
      keys_->push_back(kNameRef);
    }
  }
}

using StringRefs = std::vector<StrRef>;
static StringRefs drop_keys(const meter::Tags& tags, const StringRefs& keys) {
  std::unordered_set<StrRef> keys_set(keys.begin(), keys.end());
  StringRefs res{kNameRef};

  // add all tag keys that are not in the set of keys to be dropped
  for (const auto& tag : tags) {
    if (keys_set.find(tag.first) == end(keys_set)) {
      res.push_back(tag.first);
    }
  }
  return res;
}

TagsValuePairs KeepOrDropTags::Apply(const TagsValuePairs& valuePairs) {
  // group metrics by keys
  std::unordered_map<meter::Tags, TagsValuePairs> grouped;
  for (auto& valuePair : valuePairs) {
    auto should_keep = true;
    meter::Tags group_by_vals;

    const auto& keys = keep_ ? *keys_ : drop_keys(valuePair.tags, *keys_);

    for (auto& key : keys) {
      auto value = get_value(valuePair, key);
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

  TagsValuePairs results;
  for (auto& entry : grouped) {
    auto tags = entry.first;
    const auto& measurements_for_tags = entry.second;
    const auto& expr_results = expr_->Apply(measurements_for_tags);
    results.push_back(TagsValuePair{tags, expr_results.value});
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
