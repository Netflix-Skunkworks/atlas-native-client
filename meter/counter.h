#pragma once

#include <atomic>
#include <ostream>
#include <utility>
#include "clock.h"
#include "meter.h"

namespace atlas {
namespace meter {

template <typename T>
class CounterNumber {
 public:
  virtual void Increment() noexcept = 0;

  virtual void Add(T amount) noexcept = 0;

  virtual T Count() const noexcept = 0;
};

using Counter = CounterNumber<int64_t>;
using DCounter = CounterNumber<double>;

}  // namespace meter
}  // namespace atlas
