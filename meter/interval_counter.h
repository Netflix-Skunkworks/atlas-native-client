#pragma once

#include "counter.h"
#include "function_gauge.h"
#include "registry.h"

namespace atlas {
namespace meter {

class IntervalCounter : public Meter, public Counter {
 public:
  IntervalCounter(Registry* registry, const IdPtr& id);
  ~IntervalCounter() override = default;

  void Increment() noexcept override;
  void Add(int64_t amount) noexcept override;
  int64_t Count() const noexcept override;
  double SecondsSinceLastUpdate() const noexcept;

  std::ostream& Dump(std::ostream& os) const override;
  Measurements Measure() const override;

 private:
  std::shared_ptr<Counter> counter_;
  std::shared_ptr<int64_t> counter_updated;
  std::shared_ptr<FunctionGauge<int64_t>> gauge_;
};

}  // namespace meter
}  // namespace atlas
