#include "percentile_buckets.h"
#include <limits>

namespace atlas {
namespace meter {
namespace percentile_buckets {

#include "percentile_bucket_values.inc"

// maybe we could use the intrinsics __builtin_clz (gcc) or __lzcnt64 for clang
// but that requires cpu support for LZCNT - TODO(dmuino): check
size_t leadingZeros(uint64_t i) {
  if (i == 0) {
    return 64;
  }
  size_t n = 1;
  auto x = static_cast<uint32_t>(i >> 32);
  if (x == 0) {
    n += 32;
    x = static_cast<uint32_t>(i);
  }
  if (x >> 16 == 0) {
    n += 16;
    x <<= 16;
  }
  if (x >> 24 == 0) {
    n += 8;
    x <<= 8;
  }
  if (x >> 28 == 0) {
    n += 4;
    x <<= 4;
  }
  if (x >> 30 == 0) {
    n += 2;
    x <<= 2;
  }
  n -= x >> 31;
  return n;
}

int64_t Get(size_t i) { return kBucketValues.at(i); }

size_t IndexOf(int64_t v) {
  if (v <= 0) {
    return 0;
  }
  if (v <= 4) {
    return static_cast<size_t>(v);
  }
  auto lz = leadingZeros(static_cast<uint64_t>(v));
  size_t shift = 64 - lz - 1;
  int64_t prevPowerOf2 = (v >> shift) << shift;
  int64_t prevPowerOf4 = prevPowerOf2;
  if (shift % 2 != 0) {
    --shift;
    prevPowerOf4 >>= 1;
  }

  auto base = prevPowerOf4;
  auto delta = base / 3;
  auto offset = static_cast<size_t>((v - base) / delta);
  size_t pos = offset + kPowerOf4Index.at(shift / 2);
  return pos >= kBucketValues.size() - 1 ? kBucketValues.size() - 1 : pos + 1;
}

///
/// Compute a set of percentiles based on the counts for the buckets.
///
/// @param counts
///     Counts for each of the buckets.
///     positions must correspond to the positions of the bucket values.
/// @param pcts
///     Vector with the requested percentile values. The length must be at least
///     1 and the
///     array should be sorted. Each value, {@code v}, should adhere to {@code
///     0.0 <= v <= 100.0}.
/// @param results
///     The calculated percentile values will be written to the results vector.
void Percentiles(const std::array<int64_t, Length()>& counts,
                 const std::vector<double>& pcts,
                 std::vector<double>* results) {
  int64_t total = 0;
  for (size_t i = 0; i < kBucketValues.size(); ++i) {
    total += counts.at(i);
  }

  size_t pctIdx = 0;
  int64_t prev = 0;
  auto prevP = 0.0;
  int64_t prevB = 0;
  results->clear();
  results->resize(pcts.size());

  for (size_t i = 0; i < kBucketValues.size(); ++i) {
    auto next = prev + counts.at(i);
    auto nextP = 100.0 * next / total;
    auto nextB = kBucketValues.at(i);
    while (pctIdx < pcts.size() && nextP >= pcts[pctIdx]) {
      auto f = (pcts[pctIdx] - prevP) / (nextP - prevP);
      results->at(pctIdx) = f * (nextB - prevB) + prevB;
      ++pctIdx;
    }
    if (pctIdx >= pcts.size()) {
      break;
    }
    prev = next;
    prevP = nextP;
    prevB = nextB;
  }

  auto nextP = 100.0;
  auto nextB = std::numeric_limits<int64_t>::max();
  while (pctIdx < pcts.size()) {
    auto f = (pcts[pctIdx] - prevP) / (nextP - prevP);
    results->at(pctIdx) = f * (nextB - prevB) + prevB;
    ++pctIdx;
  }
}

double Percentile(const std::array<int64_t, Length()>& counts, double p) {
  std::vector<double> pcts{p};
  std::vector<double> results;
  Percentiles(counts, pcts, &results);
  return results[0];
}

int64_t Bucket(int64_t v) { return Get(IndexOf(v)); }

}  // namespace percentile_buckets
}  // namespace meter
}  // namespace atlas
