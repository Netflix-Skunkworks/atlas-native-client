#pragma once

#include <utility>

#include "meter.h"

namespace atlas {
namespace meter {

template <typename T>
class Gauge {
 public:
  virtual ~Gauge() = default;
  virtual double Value() const noexcept = 0;
  virtual void Update(T d) noexcept = 0;
};
}  // namespace meter
}  // namespace atlas
