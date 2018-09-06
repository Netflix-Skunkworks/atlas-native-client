#pragma once

#include <atomic>
#include <limits>
#include <ostream>
#include <utility>
#include "clock.h"
#include "id.h"
#include "measurement.h"

namespace atlas {
namespace meter {

static constexpr auto myNaN = std::numeric_limits<double>::quiet_NaN();
static constexpr std::size_t MAX_POLLER_FREQ = 4;
static constexpr int64_t MAX_IDLE_TIME =
    15 * 60 * 1000L;  // maximum time to keep monitors with no updates

using Pollers = std::vector<int64_t>;  // { 60000, 10000 } millis

class Meter {
 public:
  Meter(IdPtr id, const Clock& clock) noexcept
      : id_(std::move(id)), clock_(clock) {}

  virtual IdPtr GetId() const noexcept { return id_; }

  virtual std::ostream& Dump(std::ostream& os) const = 0;

  virtual ~Meter() noexcept = default;

  virtual Measurements Measure() const = 0;

  virtual bool HasExpired() const noexcept {
    auto last = last_updated_.load(std::memory_order::memory_order_relaxed);
    auto now = clock_.WallTime();
    return (now - last) > MAX_IDLE_TIME;
  }

  virtual bool IsUpdateable() const noexcept { return false; }

  virtual const char* GetClass() const noexcept = 0;

 protected:
  std::atomic<int64_t> last_updated_{0};
  const IdPtr id_;
  const Clock& clock_;

  inline void Updated() {
    last_updated_.store(clock_.WallTime(),
                        std::memory_order::memory_order_relaxed);
  }
};

class UpdateableMeter : public Meter {
 public:
  UpdateableMeter(IdPtr id, const Clock& clock) : Meter(id, clock) {}
  bool IsUpdateable() const noexcept override { return true; }
  virtual void Update() noexcept = 0;
};

inline std::ostream& operator<<(std::ostream& os, const Meter& meter) {
  return meter.Dump(os);
}
}  // namespace meter
}  // namespace atlas
