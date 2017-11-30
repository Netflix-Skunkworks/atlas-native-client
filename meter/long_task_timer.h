#pragma once

#include <atomic>
#include <mutex>
#include <unordered_map>
#include "meter.h"

namespace atlas {
namespace meter {
///
/// Timer intended to track a small number of long running tasks. Example would
/// be something like
/// a batch hadoop job. Though "long running" is a bit subjective the assumption
/// is that anything
/// over a minute is long running.
///
class LongTaskTimer {
 public:
  virtual ~LongTaskTimer() = default;

  /// Start keeping time for a task and return a task id that can be used to
  /// look up how long
  /// it has been running.
  virtual int64_t Start() = 0;

  /// Indicates that a given task has completed.
  ///
  /// @param task
  ///    Id for the task to stop. This should be the value returned from
  ///    {@link #start()}.
  /// @return
  ///    Duration for the task in nanoseconds. A -1 value will be returned for
  ///    an unknown task.
  virtual int64_t Stop(int64_t task) = 0;

  /// Returns the current duration for a given active task.
  ///
  /// @param task
  ///     Id for the task. This should be the value returned from {@link
  ///     #start()}.
  /// @return
  ///     Duration for the task in nanoseconds. A -1 value will be returned
  ///     for an unknown task.
  virtual int64_t Duration(int64_t task) const noexcept = 0;

  /// Returns the cumulative duration of all current tasks in nanoseconds.
  virtual int64_t Duration() const noexcept = 0;

  /// Returns the current number of tasks being executed.
  virtual int ActiveTasks() const noexcept = 0;
};
}  // namespace meter
}  // namespace atlas
