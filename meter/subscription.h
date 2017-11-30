#pragma once

#include "../interpreter/tags_value.h"
#include <thread>

namespace atlas {
namespace meter {

/// A subscription to an expression that needs to be sent to a server
/// at a given interval
struct Subscription {
  const std::string id;
  const int64_t frequency;
  const std::string expression;
};

inline bool operator==(const Subscription& lhs, const Subscription& rhs) {
  return lhs.frequency == rhs.frequency && lhs.id == rhs.id &&
         lhs.expression == rhs.expression;
}

using DataExpressionResults = std::vector<interpreter::TagsValuePairs>;

struct SubscriptionMetric {
  const std::string id;
  const Tags tags;
  const double value;
};

using SubscriptionResults = std::vector<SubscriptionMetric>;

using Subscriptions = std::vector<Subscription>;
}  // namespace meter
}  // namespace atlas
