#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace atlas {
namespace meter {
namespace percentile_buckets {

size_t IndexOf(int64_t v);
constexpr size_t Length() { return 276; }
int64_t Get(size_t i);
int64_t Bucket(int64_t v);

void Percentiles(const std::array<int64_t, Length()>& counts,
                 const std::vector<double>& pcts, std::vector<double>* results);
double Percentile(const std::array<int64_t, Length()>& counts, double p);

}  // namespace percentile_buckets
}  // namespace meter
}  // namespace atlas
