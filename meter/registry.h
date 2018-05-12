#pragma once

#include "clock.h"
#include "counter.h"
#include "distribution_summary.h"
#include "gauge.h"
#include "id.h"
#include "long_task_timer.h"
#include "meter.h"
#include "timer.h"

namespace atlas {
namespace meter {

class Registry {
 public:
  using Meters = std::vector<std::shared_ptr<Meter>>;

  virtual ~Registry() = default;

  virtual const Clock& clock() const noexcept = 0;

  virtual std::shared_ptr<Timer> timer(IdPtr id) noexcept = 0;

  virtual std::shared_ptr<Counter> counter(IdPtr id) noexcept = 0;

  virtual std::shared_ptr<DCounter> dcounter(IdPtr id) noexcept = 0;

  virtual std::shared_ptr<Gauge<double>> gauge(IdPtr id) noexcept = 0;

  virtual std::shared_ptr<Gauge<double>> max_gauge(IdPtr id) noexcept = 0;

  virtual std::shared_ptr<LongTaskTimer> long_task_timer(IdPtr id) noexcept = 0;

  virtual std::shared_ptr<DDistributionSummary> ddistribution_summary(
      IdPtr id) noexcept = 0;

  virtual std::shared_ptr<DistributionSummary> distribution_summary(
      IdPtr id) noexcept = 0;

  virtual IdPtr CreateId(std::string name, Tags tags) const = 0;

  virtual void RegisterMonitor(std::shared_ptr<Meter> meter) noexcept = 0;

  virtual Meters meters() const noexcept = 0;

  virtual Measurements measurements() noexcept = 0;

  std::shared_ptr<Counter> counter(std::string name,
                                   const Tags& tags = kEmptyTags) {
    return counter(CreateId(std::move(name), tags));
  }

  std::shared_ptr<Timer> timer(std::string name,
                               const Tags& tags = kEmptyTags) {
    return timer(CreateId(std::move(name), tags));
  }

  std::shared_ptr<Gauge<double>> gauge(std::string name,
                                       const Tags& tags = kEmptyTags) {
    return gauge(CreateId(std::move(name), tags));
  }

  std::shared_ptr<LongTaskTimer> long_task_timer(
      std::string name, const Tags& tags = kEmptyTags) {
    return long_task_timer(CreateId(std::move(name), tags));
  }

  std::shared_ptr<DistributionSummary> distribution_summary(
      std::string name, const Tags& tags = kEmptyTags) {
    return distribution_summary(CreateId(std::move(name), tags));
  }

  std::shared_ptr<DDistributionSummary> ddistribution_summary(
      std::string name, const Tags& tags = kEmptyTags) {
    return ddistribution_summary(CreateId(std::move(name), tags));
  }
};

}  // namespace meter
}  // namespace atlas
