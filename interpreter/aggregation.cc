#include "aggregation.h"
#include <cmath>

namespace atlas {
namespace interpreter {

static constexpr auto kNAN = std::numeric_limits<double>::quiet_NaN();

AggregateExpression::AggregateExpression(Aggregate aggregate,
                                         std::shared_ptr<Query> filter)
    : aggregate_(aggregate), filter_(std::move(filter)) {}

static const char* aggregate_str(Aggregate aggregate) {
  switch (aggregate) {
    case Aggregate::SUM:
      return "SUM";
    case Aggregate::COUNT:
      return "COUNT";
    case Aggregate::AVG:
      return "AVG";
    case Aggregate::MIN:
      return "MIN";
    case Aggregate::MAX:
      return "MAX";
  }
  // unreachable but gcc 4.8 on trusty complaints if we remove it
  return "UNKNOWN";
}

std::ostream& AggregateExpression::Dump(std::ostream& os) const {
  os << aggregate_str(aggregate_) << "(" << *filter_ << ")";
  return os;
}

static double sum_af(const Query& filter,
                     const TagsValuePairs& TagsValuePairs) {
  auto total = kNAN;
  for (const auto& m : TagsValuePairs) {
    if (!std::isnan(m.value) && filter.Matches(m.tags)) {
      if (std::isnan(total)) {
        total = m.value;
      } else {
        total += m.value;
      }
    }
  }
  return total;
}

static double avg_af(const Query& filter,
                     const TagsValuePairs& TagsValuePairs) {
  auto total = kNAN;
  auto count = 0;
  for (const auto& m : TagsValuePairs) {
    if (!std::isnan(m.value) && filter.Matches(m.tags)) {
      ++count;
      if (std::isnan(total)) {
        total = m.value;
      } else {
        total += m.value;
      }
    }
  }
  return count > 0 ? total / count : kNAN;
}

static double count_af(const Query& filter,
                       const TagsValuePairs& TagsValuePairs) {
  auto count = 0;
  for (const auto& m : TagsValuePairs) {
    if (!std::isnan(m.value) && filter.Matches(m.tags)) {
      ++count;
    }
  }
  return count;
}

static const double MAX_VALUE = std::numeric_limits<double>::max();
static const double LOWEST_VALUE = std::numeric_limits<double>::lowest();

static double min_af(const Query& filter, TagsValuePairs TagsValuePairs) {
  auto mn = MAX_VALUE;
  for (const auto& m : TagsValuePairs) {
    if (!std::isnan(m.value) && filter.Matches(m.tags)) {
      mn = std::min(mn, m.value);
    }
  }
  return mn == MAX_VALUE ? kNAN : mn;
}

static double max_af(const Query& filter, TagsValuePairs TagsValuePairs) {
  auto mx = LOWEST_VALUE;
  for (const auto& m : TagsValuePairs) {
    if (!std::isnan(m.value) && filter.Matches(m.tags)) {
      mx = std::max(mx, m.value);
    }
  }
  return mx == LOWEST_VALUE ? kNAN : mx;
}

TagsValuePair AggregateExpression::Apply(
    const TagsValuePairs& tagsValuePairs) const {
  double aggregate =
      kNAN;  // quiets warning on gcc 4.8 about maybe uninitialized
  switch (aggregate_) {
    case Aggregate::SUM:
      aggregate = sum_af(*filter_, tagsValuePairs);
      break;
    case Aggregate::COUNT:
      aggregate = count_af(*filter_, tagsValuePairs);
      break;
    case Aggregate::AVG:
      aggregate = avg_af(*filter_, tagsValuePairs);
      break;
    case Aggregate::MIN:
      aggregate = min_af(*filter_, tagsValuePairs);
      break;
    case Aggregate::MAX:
      aggregate = max_af(*filter_, tagsValuePairs);
      break;
  }
  return TagsValuePair{filter_->Tags(), aggregate};
}

// utility functions
std::unique_ptr<AggregateExpression> aggregation::count(
    std::shared_ptr<Query> filter) {
  return std::make_unique<AggregateExpression>(Aggregate::COUNT, filter);
}

std::unique_ptr<AggregateExpression> aggregation::sum(
    std::shared_ptr<Query> filter) {
  return std::make_unique<AggregateExpression>(Aggregate::SUM, filter);
}

std::unique_ptr<AggregateExpression> aggregation::min(
    std::shared_ptr<Query> filter) {
  return std::make_unique<AggregateExpression>(Aggregate::MIN, filter);
}

std::unique_ptr<AggregateExpression> aggregation::max(
    std::shared_ptr<Query> filter) {
  return std::make_unique<AggregateExpression>(Aggregate::MAX, filter);
}

std::unique_ptr<AggregateExpression> aggregation::avg(
    std::shared_ptr<Query> filter) {
  return std::make_unique<AggregateExpression>(Aggregate::AVG, filter);
}
}  // namespace interpreter
}  // namespace atlas
