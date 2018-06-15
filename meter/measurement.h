#pragma once

#include "id.h"
#include <cmath>
#include <vector>

namespace atlas {
namespace meter {

struct Measurement {
  IdPtr id;
  int64_t timestamp;
  double value;

  bool operator==(const Measurement& other) const {
    return other.timestamp == timestamp &&
           std::abs(value - other.value) < 1e-9 && *id == *(other.id);
  }
};

inline std::ostream& operator<<(std::ostream& os, const Measurement& m) {
  os << "Measurement{" << *(m.id) << "," << m.timestamp << "," << m.value
     << "}";
  return os;
}

inline Measurement factor_measurement(Measurement measurement,
                                      double conv_factor) {
  return Measurement{measurement.id, measurement.timestamp,
                     measurement.value * conv_factor};
}

using Measurements = std::vector<Measurement>;
}  // namespace meter
}  // namespace atlas
