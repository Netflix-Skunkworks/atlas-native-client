#pragma once

#include <cstdint>

namespace atlas {
namespace meter {
class DistributionSummary {
 public:
  virtual void Record(int64_t amount) noexcept = 0;
  virtual int64_t Count() const noexcept = 0;
  virtual int64_t TotalAmount() const noexcept = 0;
};
}  // namespace meter
}  // namespace atlas
