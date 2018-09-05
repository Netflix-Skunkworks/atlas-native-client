#pragma once

#include "default_counter.h"
#include "default_max_gauge.h"
#include "timer.h"

namespace atlas {
namespace meter {

class DefaultTimer : public Meter, public Timer {
 public:
  DefaultTimer(const IdPtr& id, const Clock& clock, int64_t freq_millis);

  Measurements Measure() const override;

  std::ostream& Dump(std::ostream& os) const override;

  void Record(std::chrono::nanoseconds nanos) override;

  int64_t Count() const noexcept override;

  int64_t TotalTime() const noexcept override;

  const char* GetClass() const noexcept override { return "DefaultTimer"; }

 private:
  // used to conform to the API for timer
  std::atomic<int64_t> count_;
  std::atomic<int64_t> total_time_;

  DefaultCounter sub_count_;
  DefaultDoubleCounter sub_total_sq_;
  DefaultCounter sub_total_time_;
  DefaultMaxGaugeInt sub_max_;
};
}  // namespace meter
}  // namespace atlas
