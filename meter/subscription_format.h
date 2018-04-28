#pragma once

#include "subscription.h"
#include "id_format.h"
#include <fmt/ostream.h>

namespace atlas {
namespace meter {

std::ostream& operator<<(std::ostream& os, const Subscription& subscription) {
  os << "Subscription{" << subscription.id << "," << subscription.frequency
     << "," << subscription.expression << "}";
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const std::vector<Subscription>& subscriptions) {
  os << "Subscriptions:[\n";
  for (const auto& s : subscriptions) {
    os << "\t{" << s.id << "," << s.frequency << "," << s.expression << "\n";
  }
  os << ']';
  return os;
}

std::ostream& operator<<(std::ostream& os, const SubscriptionMetric& s) {
  os << "SubMetric{id=" << s.id << ",tags=" << s.tags << ",v=" << s.value
     << "}";
  return os;
}

}  // namespace meter
}  // namespace atlas
