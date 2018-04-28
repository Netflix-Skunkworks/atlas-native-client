#include "atlas_registry.h"
#include "../interpreter/group_by.h"
#include "../interpreter/interpreter.h"
#include "../util/logger.h"
#include "default_counter.h"
#include "default_distribution_summary.h"
#include "default_gauge.h"
#include "default_max_gauge.h"
#include "default_long_task_timer.h"
#include "default_timer.h"

#include <numeric>
#include <sstream>

using atlas::util::Logger;

namespace atlas {
namespace meter {

SystemClock system_clock;

// we mostly use this class to avoid adding an external dependency to
// the interpreter from our public headers
class AtlasRegistry::impl {
 public:
  explicit impl() noexcept {}

  std::shared_ptr<Meter> GetMeter(IdPtr id) noexcept {
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
    std::lock_guard<std::mutex> lock(meters_mutex);

    Meters res;
    res.reserve(meters_.size());

    for (auto m : meters_) {
      res.push_back(m.second);
    }
    return res;
  }

 private:
  mutable std::mutex meters_mutex{};
  std::unordered_map<IdPtr, std::shared_ptr<Meter>> meters_{};
};

const Clock& AtlasRegistry::clock() const noexcept { return *clock_; }

IdPtr AtlasRegistry::CreateId(std::string name, Tags tags) const noexcept {
  return std::make_shared<Id>(name, tags);
}

std::shared_ptr<Meter> AtlasRegistry::GetMeter(IdPtr id) noexcept {
  return impl_->GetMeter(id);
}

// only insert if it doesn't exist, otherwise return the existing meter
std::shared_ptr<Meter> AtlasRegistry::InsertIfNeeded(
    std::shared_ptr<Meter> meter) noexcept {
  return impl_->InsertIfNeeded(meter);
}

std::shared_ptr<Counter> AtlasRegistry::counter(IdPtr id) noexcept {
  return CreateAndRegisterAsNeeded<DefaultCounter>(id, clock(), freq_millis_);
}

std::shared_ptr<DCounter> AtlasRegistry::dcounter(IdPtr id) noexcept {
  return CreateAndRegisterAsNeeded<DefaultDoubleCounter>(id, clock(),
                                                         freq_millis_);
}

std::shared_ptr<Timer> AtlasRegistry::timer(IdPtr id) noexcept {
  return CreateAndRegisterAsNeeded<DefaultTimer>(id, clock(), freq_millis_);
}

std::shared_ptr<Gauge<double>> AtlasRegistry::gauge(IdPtr id) noexcept {
  return CreateAndRegisterAsNeeded<DefaultGauge>(id, clock());
}

std::shared_ptr<LongTaskTimer> AtlasRegistry::long_task_timer(
    IdPtr id) noexcept {
  return CreateAndRegisterAsNeeded<DefaultLongTaskTimer>(id, clock());
}

std::shared_ptr<DDistributionSummary> AtlasRegistry::ddistribution_summary(
    IdPtr id) noexcept {
  return CreateAndRegisterAsNeeded<DefaultDoubleDistributionSummary>(
      id, clock(), freq_millis_);
}

std::shared_ptr<DistributionSummary> AtlasRegistry::distribution_summary(
    IdPtr id) noexcept {
  return CreateAndRegisterAsNeeded<DefaultDistributionSummary>(id, clock(),
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

/*
  raw_measurements_size->Update(all.size());
  main_measurements_size->Update(result->size());
 */

/*

 */

AtlasRegistry::AtlasRegistry(int64_t freq_millis, const Clock* clock) noexcept
    : impl_{std::make_unique<impl>()},
      clock_{clock},
      freq_millis_{freq_millis},
      freq_tags{} {
  auto freq_value = fmt::format("{:02d}s", freq_millis / 1000);
  freq_tags.add("id", freq_value.c_str());
  meters_size = gauge(CreateId("atlas.numMeters", freq_tags));
}

Measurements AtlasRegistry::measurements() const noexcept {
  Measurements res;

  // quickly create a copy of the meters to avoid locking while we get the
  // measurements
  const auto all_meters = meters();

  // attempt to guess how big the resulting measurements will be
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
  return res;
}

std::shared_ptr<Gauge<double>> AtlasRegistry::max_gauge(IdPtr id) noexcept {
  return CreateAndRegisterAsNeeded<DefaultMaxGauge<double>>(id, clock(),
                                                            freq_millis_);
}

AtlasRegistry::~AtlasRegistry() = default;

}  // namespace meter
}  // namespace atlas
