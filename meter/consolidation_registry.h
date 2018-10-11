#pragma once

#include <mutex>
#include <ska/flat_hash_map.hpp>
#include "registry.h"
#include "stepnumber.h"
namespace atlas {

namespace meter {

class ConsolidatedValue {
 public:
  static constexpr auto kMinValue = std::numeric_limits<double>::lowest();
  ConsolidatedValue(bool is_gauge, int update_multiple, int64_t step_ms,
                    const Clock* clock)
      : is_gauge_(is_gauge),
        update_multiple_(update_multiple),
        v(StepNumber<double>(is_gauge ? kMinValue : 0.0, step_ms, clock)),
        marked_{false} {}
  bool has_value() const {
    auto value = v.Poll();
    return is_gauge_ ? value > kMinValue : value > 0;
  }
  double value() const { return v.Poll(); }
  void update(double value) {
    if (is_gauge_) {
      v.UpdateCurrentMax(value);
    } else {
      v.Add(value / update_multiple_);
    }
  }
  bool marked() const noexcept { return marked_; }
  void set_marked(bool marked) noexcept { marked_ = marked; }

 private:
  bool is_gauge_;
  int update_multiple_;
  StepNumber<double> v;
  bool marked_;
};

class ConsolidationRegistry {
 public:
  ConsolidationRegistry(int64_t update_frequency, int64_t reporting_frequency,
                        const Clock* clock) noexcept;
  void update_from(const Measurements& measurements) noexcept;
  Measurements measurements(int64_t timestamp) const noexcept;
  size_t size() const noexcept {
    std::lock_guard<std::mutex> guard{mutex};
    return my_values.size();
  }

 private:
  int64_t update_frequency_;
  int64_t reporting_frequency_;
  WrappedClock clock_;
  using values_map = ska::flat_hash_map<IdPtr, ConsolidatedValue>;
  mutable values_map my_values;
  mutable std::mutex mutex;
};

}  // namespace meter

}  // namespace atlas
