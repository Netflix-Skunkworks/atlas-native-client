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

  virtual std::shared_ptr<DistributionSummary> distribution_summary(
      IdPtr id) noexcept = 0;

  virtual IdPtr CreateId(std::string name, Tags tags) = 0;

  virtual void RegisterMonitor(std::shared_ptr<Meter> meter) noexcept = 0;

  virtual Meters meters() const noexcept = 0;

  virtual Pollers& pollers() noexcept = 0;
};

}  // namespace meter
}  // namespace atlas
