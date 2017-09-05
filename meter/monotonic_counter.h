#pragma once

#include "meter.h"
#include "registry.h"

namespace atlas {
namespace meter {

class MonotonicCounter : public Meter {
 public:
  MonotonicCounter(Registry* registry, IdPtr id);
  Measurements MeasuresForPoller(size_t poller_idx) const override;
  std::ostream& Dump(std::ostream& os) const override;
  void Set(int64_t amount) noexcept;
  int64_t Count() const noexcept;

 private:
  std::atomic<int64_t> value_;
  Registry* registry_;
  std::shared_ptr<Counter> counter_;
};

}  // namespace meter
}  // namespace atlas
