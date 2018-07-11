#pragma once

#include "registry.h"
namespace atlas {

namespace meter {

class ConsolidationRegistry {
 public:
  ConsolidationRegistry(int64_t update_frequency,
                        int64_t reporting_frequency) noexcept;
  void update_from(const Measurements& measurements) noexcept;
  Measurements measurements() const noexcept;

 private:
  int64_t update_multiple;
  mutable std::unordered_map<IdPtr, Measurement> my_measures;
  mutable std::mutex mutex;
};

}  // namespace meter

}  // namespace atlas
