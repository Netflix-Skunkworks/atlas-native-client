#include "atlas_registry.h"
#include "../interpreter/group_by.h"
#include "../interpreter/interpreter.h"
#include "../util/logger.h"
#include "default_counter.h"
#include "default_distribution_summary.h"
#include "default_gauge.h"
#include "default_long_task_timer.h"
#include "default_max_gauge.h"
#include "default_timer.h"

#include <numeric>
#include <sstream>
#include <utility>

namespace atlas {
namespace meter {

// we mostly use this class to avoid adding an external dependency to
// the interpreter from our public headers
class AtlasRegistry::impl {
 public:
  explicit impl() noexcept {}

  std::shared_ptr<Meter> GetMeter(const IdPtr& id) noexcept {
    std::lock_guard<std::mutex> lock(meters_mutex);
    auto maybe_meter = meters_.find(id);
    if (maybe_meter != meters_.end()) {
      return maybe_meter->second;
    }
    return std::shared_ptr<Meter>(nullptr);
  }

  // only insert if it doesn't exist, otherwise return the existing meter
  std::shared_ptr<Meter> InsertIfNeeded(std::shared_ptr<Meter> meter) noexcept {
    std::lock_guard<std::mutex> lock(meters_mutex);
    auto insert_result = meters_.insert(std::make_pair(meter->GetId(), meter));
    auto ret = insert_result.first->second;
    return ret;
  }

  Meters GetMeters() const noexcept {
    Meters res;
    res.reserve(meters_.size());
    for (auto& m : meters_) {
      res.push_back(m.second);
    }
    return res;
  }

  int64_t ExpireMeters() noexcept {
    int64_t expired = 0;
    std::lock_guard<std::mutex> lock(meters_mutex);

    auto it = meters_.begin();
    while (it != meters_.end()) {
      if (it->second->HasExpired() && it->second.use_count() == 1) {
        ++expired;
        it = meters_.erase(it);
      } else {
        ++it;
      }
    }
    return expired;
  }

 private:
  mutable std::mutex meters_mutex{};
  std::unordered_map<IdPtr, std::shared_ptr<Meter>> meters_{};
};

const Clock& AtlasRegistry::clock() const noexcept { return *clock_; }

IdPtr AtlasRegistry::CreateId(std::string name, Tags tags) const noexcept {
  return std::make_shared<Id>(name, tags);
}

std::shared_ptr<Meter> AtlasRegistry::GetMeter(const IdPtr& id) noexcept {
  return impl_->GetMeter(id);
}

// only insert if it doesn't exist, otherwise return the existing meter
std::shared_ptr<Meter> AtlasRegistry::InsertIfNeeded(
    std::shared_ptr<Meter> meter) noexcept {
  return impl_->InsertIfNeeded(std::move(meter));
}

std::shared_ptr<Counter> AtlasRegistry::counter(IdPtr id) noexcept {
  return CreateAndRegisterAsNeeded<DefaultCounter>(id, *clock_, freq_millis_);
}

std::shared_ptr<DCounter> AtlasRegistry::dcounter(IdPtr id) noexcept {
  return CreateAndRegisterAsNeeded<DefaultDoubleCounter>(id, *clock_,
                                                         freq_millis_);
}

std::shared_ptr<Timer> AtlasRegistry::timer(IdPtr id) noexcept {
  return CreateAndRegisterAsNeeded<DefaultTimer>(id, *clock_, freq_millis_);
}

std::shared_ptr<Gauge<double>> AtlasRegistry::gauge(IdPtr id) noexcept {
  return CreateAndRegisterAsNeeded<DefaultGauge>(id, *clock_);
}

std::shared_ptr<LongTaskTimer> AtlasRegistry::long_task_timer(
    IdPtr id) noexcept {
  return CreateAndRegisterAsNeeded<DefaultLongTaskTimer>(id, *clock_);
}

std::shared_ptr<DDistributionSummary> AtlasRegistry::ddistribution_summary(
    IdPtr id) noexcept {
  return CreateAndRegisterAsNeeded<DefaultDoubleDistributionSummary>(
      id, *clock_, freq_millis_);
}

std::shared_ptr<DistributionSummary> AtlasRegistry::distribution_summary(
    IdPtr id) noexcept {
  return CreateAndRegisterAsNeeded<DefaultDistributionSummary>(id, *clock_,
                                                               freq_millis_);
}

Registry::Meters AtlasRegistry::meters() const noexcept {
  const auto& meters = impl_->GetMeters();
  meters_size->Update(meters.size());
  return meters;
}

void AtlasRegistry::RegisterMonitor(std::shared_ptr<Meter> meter) noexcept {
  InsertIfNeeded(meter);
}

AtlasRegistry::AtlasRegistry(int64_t freq_millis, const Clock* clock) noexcept
    : impl_{std::make_unique<impl>()},
      clock_{clock},
      freq_millis_{freq_millis} {
  auto freq_value = util::secs_for_millis(freq_millis);
  freq_tags.add("id", freq_value.c_str());
  // avoid using virtual functions from within the constructor
  // (do not call gauge, counter here)
  meters_size = CreateAndRegisterAsNeeded<DefaultGauge>(
      std::make_shared<Id>("atlas.numMeters", freq_tags), *clock_);
  expired_meters = CreateAndRegisterAsNeeded<DefaultCounter>(
      std::make_shared<Id>("atlas.expiredMeters", freq_tags), *clock_,
      freq_millis);
}

Measurements AtlasRegistry::measurements() noexcept {
  Measurements res;

  {
    // quickly create a copy of the meters to avoid locking while we get the
    // measurements
    const auto all_meters = meters();

    // attempt to guess how big the resulting measurements will be.
    // timers / distribution summaries = 4x, counters = 1x
    // so we pick 2x
    res.reserve(all_meters.size() * 2);

    // now get a measurement from each meter that is involved in the given query
    for (const auto& m : all_meters) {
      if (!m->HasExpired()) {
        if (m->IsUpdateable()) {
          std::static_pointer_cast<UpdateableMeter>(m)->Update();
        }
        const auto measurements = m->Measure();
        if (!measurements.empty()) {
          std::move(measurements.begin(), measurements.end(),
                    std::back_inserter(res));
        }
      }
    }
  }
  // destruct our copy of the meters before calling expire_meters, this
  // will reduce the refcount
  expire_meters();

  return res;
}

std::shared_ptr<Gauge<double>> AtlasRegistry::max_gauge(IdPtr id) noexcept {
  return CreateAndRegisterAsNeeded<DefaultMaxGauge<double>>(id, clock(),
                                                            freq_millis_);
}

void AtlasRegistry::expire_meters() noexcept {
  expired_meters->Add(impl_->ExpireMeters());
}

AtlasRegistry::~AtlasRegistry() = default;

}  // namespace meter
}  // namespace atlas
