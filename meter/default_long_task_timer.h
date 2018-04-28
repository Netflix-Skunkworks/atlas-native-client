#pragma once

#include "long_task_timer.h"

namespace atlas {
namespace meter {

class DefaultLongTaskTimer : public Meter, public LongTaskTimer {
 public:
  DefaultLongTaskTimer(IdPtr id, const Clock& clock);

  Measurements Measure() const override;

  std::ostream& Dump(std::ostream& os) const override;

  int64_t Start() override;

  int64_t Stop(int64_t task) override;

  int64_t Duration(int64_t task) const noexcept override;

  int64_t Duration() const noexcept override;

  int ActiveTasks() const noexcept override;

  // Long task timers don't expire
  bool HasExpired() const noexcept override { return false; }

 private:
  static const int EXPECTED_TASKS = 8;
  std::atomic<int64_t> next_;
  mutable std::mutex tasks_mutex_;
  std::unordered_map<int64_t, int64_t> tasks_;
};
}  // namespace meter
}  // namespace atlas
