#pragma once

#include "../util/config.h"
#include "registry.h"
#include "stepnumber.h"
#include "subscription.h"
#include <array>
#include <mutex>

namespace atlas {

namespace meter {

class AtlasRegistry : public Registry {
 public:
  AtlasRegistry(int64_t freq_millis, const Clock* clock) noexcept;

  AtlasRegistry(const AtlasRegistry& registry) = delete;
  ~AtlasRegistry() override;

  const Clock& clock() const noexcept override;

  IdPtr CreateId(std::string name, Tags tags) const noexcept override;

  std::shared_ptr<Counter> counter(IdPtr id) noexcept override;

  std::shared_ptr<DCounter> dcounter(IdPtr id) noexcept override;

  std::shared_ptr<Timer> timer(IdPtr id) noexcept override;

  std::shared_ptr<Gauge<double>> gauge(IdPtr id) noexcept override;

  std::shared_ptr<LongTaskTimer> long_task_timer(IdPtr id) noexcept override;

  std::shared_ptr<DistributionSummary> distribution_summary(
      IdPtr id) noexcept override;

  std::shared_ptr<DDistributionSummary> ddistribution_summary(
      IdPtr id) noexcept override;

  void RegisterMonitor(std::shared_ptr<Meter> meter) noexcept override;

  Meters meters() const noexcept override;

  Measurements measurements() noexcept override;

  std::shared_ptr<Gauge<double>> max_gauge(IdPtr id) noexcept override;

 protected:
  void expire_meters() noexcept;

 private:
  const Clock* clock_;
  int64_t freq_millis_;
  Tags freq_tags;
  mutable std::shared_ptr<Gauge<double>> meters_size;
  mutable std::shared_ptr<Counter> expired_meters;
  mutable std::mutex meters_mutex{};
  ska::flat_hash_map<IdPtr, std::shared_ptr<Meter>> meters_{};

  std::shared_ptr<Meter> insert_if_needed(
      std::shared_ptr<Meter> meter) noexcept;
  void log_type_error(const Id& id, const char* prev_class,
                      const char* attempted_class) const noexcept;

  template <typename M, typename... Args>
  std::shared_ptr<M> create_and_register_as_needed(IdPtr id,
                                                   Args&&... args) noexcept {
    std::shared_ptr<M> new_meter_ptr{
        std::make_shared<M>(std::move(id), std::forward<Args>(args)...)};
    auto meter_ptr = insert_if_needed(new_meter_ptr);
    if (strcmp(meter_ptr->GetClass(), new_meter_ptr->GetClass()) != 0) {
      log_type_error(*meter_ptr->GetId(), meter_ptr->GetClass(),
                     new_meter_ptr->GetClass());
      return new_meter_ptr;
    }
    return std::static_pointer_cast<M>(meter_ptr);
  }
};

}  // namespace meter
}  // namespace atlas
