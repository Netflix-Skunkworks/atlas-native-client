#include "default_long_task_timer.h"

#include "../util/logger.h"
#include "statistic.h"
#include <utility>

namespace atlas {
namespace meter {

DefaultLongTaskTimer::DefaultLongTaskTimer(IdPtr id, const Clock& clock)
    : Meter(std::move(id), clock), next_(0), tasks_(EXPECTED_TASKS) {}

int64_t DefaultLongTaskTimer::Start() {
  std::lock_guard<std::mutex> lock(tasks_mutex_);
  const auto task = next_++;
  tasks_[task] = clock_.MonotonicTime();
  return task;
}

int64_t DefaultLongTaskTimer::Duration(int64_t task) const noexcept {
  std::lock_guard<std::mutex> lock(tasks_mutex_);
  const auto now = clock_.MonotonicTime();
  const auto start_time = tasks_.find(task);
  const auto elapsed_time =
      start_time != tasks_.end() ? now - (*start_time).second : -1;
  if (elapsed_time < 0) {
    util::Logger()->info("Unknown task id {}", task);
  }

  return elapsed_time;
}

int64_t DefaultLongTaskTimer::Stop(int64_t task) {
  auto d = Duration(task);
  std::lock_guard<std::mutex> lock(tasks_mutex_);
  if (d >= 0) {
    tasks_.erase(task);
  }
  return d;
}

int64_t DefaultLongTaskTimer::Duration() const noexcept {
  std::lock_guard<std::mutex> lock(tasks_mutex_);
  const auto now = clock_.MonotonicTime();
  int64_t total_duration = 0;
  for (const auto &value : tasks_) {
    const auto nanos = now - value.second;
    total_duration += nanos;
  }
  return total_duration;
}

static const double NANOS_IN_SECS = 1e9;

Measurements DefaultLongTaskTimer::Measure() const {
  const auto now = clock_.WallTime();
  const auto duration_in_secs = Duration() / NANOS_IN_SECS;
  double active = ActiveTasks();
  return Measurements{
      Measurement{id_->WithTag(statistic::activeTasks), now, active},
      Measurement{id_->WithTag(statistic::duration), now, duration_in_secs}};
}

std::ostream& DefaultLongTaskTimer::Dump(std::ostream& os) const {
  os << "DefaultLongTaskTimer(" << GetId() << " activeTasks=" << ActiveTasks()
     << " duration=" << Duration() / NANOS_IN_SECS << " seconds)";
  return os;
}

int DefaultLongTaskTimer::ActiveTasks() const noexcept {
  std::lock_guard<std::mutex> lock(tasks_mutex_);
  return static_cast<int>(tasks_.size());
}

}  // namespace meter
}  // namespace atlas
