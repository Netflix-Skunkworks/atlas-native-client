#include "group_by.h"
#include "../util/logger.h"
#include "singleton_value_expr.h"

namespace atlas {
namespace interpreter {

using util::intern_str;
using util::Logger;

namespace expression {

std::shared_ptr<MultipleResults> GetMultipleResults(
    const std::shared_ptr<Expression>& e) {
  if (e->GetType() == ExpressionType::MultipleResults) {
    return std::static_pointer_cast<MultipleResults>(e);
  }

  if (e->GetType() == ExpressionType::ValueExpression) {
    auto ve = std::static_pointer_cast<ValueExpression>(e);
    return std::make_shared<SingletonValueExpr>(ve);
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

TagsValuePairs GroupBy::Apply(const TagsValuePairs& measurements) {
  // group metrics by keys
  std::unordered_map<meter::Tags, TagsValuePairs> grouped;
  for (auto& tagsValuePair : measurements) {
    auto should_keep = true;
    meter::Tags group_by_vals;
    for (auto& key : *keys_) {
      auto value = get_value(*tagsValuePair, key);
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

  auto results = TagsValuePairs();
  for (auto& entry : grouped) {
    auto tags = entry.first;
    const auto& pairs_for_key = entry.second;
    const auto& expr_results = expr_->Apply(pairs_for_key);
    auto v = expr_results->value();
    if (std::isnan(v)) {
      continue;
    }
    tags.add_all(expr_results->all_tags());
    results.emplace_back(
        TagsValuePair::of(std::move(tags), expr_results->value()));
  }

  return results;
}
}  // namespace interpreter
}  // namespace atlas
