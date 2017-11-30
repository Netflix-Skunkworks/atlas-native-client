#pragma once

#include <chrono>

namespace atlas {
namespace meter {
class Timer {
 public:
  virtual ~Timer() = default;

  virtual void Record(std::chrono::nanoseconds nanos) = 0;

  virtual void Record(int64_t nanos) {
    Record(std::chrono::nanoseconds(nanos));
  }

  virtual int64_t Count() const noexcept = 0;

  virtual int64_t TotalTime() const noexcept = 0;
};
}  // namespace meter
}  // namespace atlas
