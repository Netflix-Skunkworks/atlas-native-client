#pragma once

#include "bucket_functions.h"
#include "meter.h"
#include "registry.h"
#include "timer.h"

namespace atlas {
namespace meter {
class BucketTimer : public Meter, public Timer {
 public:
  BucketTimer(Registry* registry, IdPtr id, BucketFunction bucket_function);
  std::ostream& Dump(std::ostream& os) const override;
  Measurements MeasuresForPoller(size_t poller_idx) const override;
  void Record(std::chrono::nanoseconds duration) override;
  int64_t Count() const noexcept override;
  int64_t TotalTime() const noexcept override;

 private:
  Registry* registry_;
  BucketFunction bucket_function_;
};
}  // namespace meter
}  // namespace atlas
