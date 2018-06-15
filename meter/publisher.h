#pragma once

#include "../interpreter/tags_value.h"
#include "../meter/subscription.h"
#include "../util/config.h"
#include "../util/http.h"
#include "registry.h"

namespace atlas {
namespace meter {

class Publisher {
 public:
  explicit Publisher(std::shared_ptr<Registry> registry) noexcept
      : registry_(std::move(registry)) {}

  void PushMeasurements(const util::Config& config, int64_t now_millis,
                        const interpreter::TagsValuePairs& measurements) const
      noexcept;
  void SendSubscriptions(const util::Config& config, int64_t freq_millis,
                         const SubscriptionResults& sub_results) const noexcept;

 private:
  std::shared_ptr<Registry> registry_;

  void batch_to_lwc(
      const util::http& client, const util::Config& config, int64_t freq_millis,
      const meter::SubscriptionResults::const_iterator& first,
      const meter::SubscriptionResults::const_iterator& last) const noexcept;
};

}  // namespace meter
}  // namespace atlas
