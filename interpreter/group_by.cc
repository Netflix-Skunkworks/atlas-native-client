#include "group_by.h"
#include "../util/logger.h"

namespace atlas {
namespace interpreter {

using util::Logger;
using util::intern_str;

namespace expression {

std::shared_ptr<MultipleResults> GetMultipleResults(
    std::shared_ptr<Expression> e) {
  if (e->GetType() == ExpressionType::MultipleResults) {
    return std::static_pointer_cast<MultipleResults>(e);
  }

  if (e->GetType() == ExpressionType::ValueExpression) {
    auto ve = std::static_pointer_cast<ValueExpression>(e);
    return std::make_shared<All>(ve);
  }

  Logger()->error(
      "Expecting a GroupBy clause or ValueExpression. Got {} instead.", *e);
  return std::shared_ptr<MultipleResults>(nullptr);
}
}  // namespace expression

using ::atlas::meter::Measurements;

GroupBy::GroupBy(const List& keys, std::shared_ptr<ValueExpression> expr)
    : keys_(keys.ToStrings()), expr_(std::move(expr)) {}

std::ostream& GroupBy::Dump(std::ostream& os) const {
  os << "GroupBy(" << keys_str(*keys_) << "," << *expr_ << ")";
  return os;
}

TagsValuePairs GroupBy::Apply(const TagsValuePairs& tagsValuePairs) {
  // group metrics by keys
  std::unordered_map<meter::Tags, TagsValuePairs> grouped;
  for (auto& tagsValuePair : tagsValuePairs) {
    auto should_keep = true;
    meter::Tags group_by_vals;
    for (auto& key : *keys_) {
      auto value = get_value(tagsValuePair, key);
      if (value) {
        group_by_vals.add(key, intern_str(value.get()));
      } else {
        should_keep = false;
        break;
      }
    }
    if (should_keep) {
      grouped[group_by_vals].push_back(tagsValuePair);
    }
  }

  TagsValuePairs results;
  for (auto& entry : grouped) {
    auto tags = entry.first;
    const auto& pairs_for_key = entry.second;
    const auto& expr_results = expr_->Apply(pairs_for_key);
    tags.add_all(expr_results.tags);

    results.push_back(TagsValuePair{tags, expr_results.value});
  }

  return results;
}
}  // namespace interpreter
}  // namespace atlas
