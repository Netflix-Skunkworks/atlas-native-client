#pragma once

#include "gauge.h"
#include "registry.h"
#include <functional>

namespace atlas {
namespace meter {

/// A gauge that will compute its value by invoking a function
/// on a given object
template <typename T>
class FunctionGauge : public UpdateableMeter {
 public:
  FunctionGauge(IdPtr id, const Clock& clock, std::weak_ptr<T> ptr,
                const std::function<double(T)>& f) noexcept
      : UpdateableMeter{WithDefaultGaugeTags(id), clock},
        ptr_{ptr},
        f_{f},
        value_{kNAN} {}

  ~FunctionGauge() override = default;

  bool HasExpired() const noexcept override { return ptr_.expired(); }

  double Value() const noexcept {
    auto ptr = ptr_.lock();
    return ptr ? f_(*ptr) : kNAN;
  }

  std::ostream& Dump(std::ostream& os) const override {
    auto ptr = ptr_.lock();
    os << "FunctionGauge{" << *id_ << ",obj=" << *ptr << ",value=" << Value()
       << "}";
    return os;
  }

  Measurements Measure() const override {
    return Measurements{Measurement{id_, clock_.WallTime(), value_}};
  }

  void Update() noexcept override { value_ = Value(); }

  const char* GetClass() const noexcept override { return "FunctionGauge"; }

 private:
  static constexpr auto kNAN = std::numeric_limits<double>::quiet_NaN();
  std::weak_ptr<T> ptr_;
  std::function<double(T)> f_;
  std::atomic<double> value_;
};
}  // namespace meter
}  // namespace atlas
