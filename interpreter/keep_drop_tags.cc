#include "keep_drop_tags.h"
#include "../meter/id.h"
#include "../meter/measurement.h"
#include <algorithm>
#include <unordered_set>

namespace atlas {
namespace interpreter {

using ::atlas::meter::Measurements;

const static std::string name_str{"name"};
KeepOrDropTags::KeepOrDropTags(const List& keys,
                               std::shared_ptr<ValueExpression> expr, bool keep)
    : keys_(keys.ToStrings()), expr_(std::move(expr)), keep_(keep) {
  if (keep) {
    // make sure we keep name, it's required
    if (std::find(keys_->begin(), keys_->end(), name_str) == keys_->end()) {
      keys_->push_back(name_str);
    }
  }
}

static Strings drop_keys(const meter::Tags& tags, const Strings& keys) {
  std::unordered_set<std::string> keys_set(keys.begin(), keys.end());
  Strings res{name_str};

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
  std::map<meter::Tags, TagsValuePairs> grouped;
  for (auto& valuePair : valuePairs) {
    auto should_keep = true;
    meter::Tags group_by_vals;

    const auto& keys = keep_ ? *keys_ : drop_keys(valuePair.tags, *keys_);

    for (auto& key : keys) {
      auto value = get_value(valuePair, key);
      if (value) {
        group_by_vals[key] = value.get();
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
