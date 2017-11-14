#include "bucket_functions.h"

#include <array>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace atlas {
namespace meter {
namespace bucket_functions {
namespace {

class Bucket {
 public:
  Bucket(std::string name, int64_t upper_boundary) noexcept
      : name_{std::move(name)}, upper_boundary_{upper_boundary} {}

  std::string Name() const noexcept { return name_; }
  int64_t UpperBoundary() const noexcept { return upper_boundary_; }

 private:
  std::string name_;
  int64_t upper_boundary_;
};

using Buckets = std::vector<Bucket>;
class BucketsFunction {
 public:
  BucketsFunction(Buckets&& buckets, std::string&& fallback) noexcept
      : buckets_{buckets}, fallback_{fallback} {}

  std::string operator()(int64_t amount) {
    for (const auto& b : buckets_) {
      if (amount <= b.UpperBoundary()) {
        return b.Name();
      }
    }
    return fallback_;
  }

 private:
  Buckets buckets_;
  std::string fallback_;
};

/// Format a value as a bucket label
class ValueFormatter {
 public:
  ///
  /// Create a new instance.
  ///
  /// @param max
  ///     Maximum value intended to be passed into the apply method. Max value
  ///     is in nanoseconds.
  /// @param width
  ///     Number of digits to use for the numeric part of the label.
  /// @param suffix
  ///     Unit suffix appended to the label.
  /// @param nanos_factor
  ///     Factor conversion from value to unit for label.
  ///
  ValueFormatter(int64_t max, int width, std::string suffix,
                 int64_t factor) noexcept
      : max_{max}, width_{width}, suffix_{std::move(suffix)}, factor_{factor} {}

  /// Get the bucket label for the value v
  std::string GetLabel(int64_t v) const noexcept {
    std::stringstream ss;
    auto unit = v / factor_;
    ss << std::setw(width_) << std::setfill('0') << unit << suffix_;
    return ss.str();
  }

  /// Return a new bucket for the specified value
  Bucket NewBucket(int64_t v) const noexcept { return Bucket{GetLabel(v), v}; }

  int64_t Max() const noexcept { return max_; }

 private:
  const int64_t max_;
  const int width_;
  const std::string suffix_;
  const int64_t factor_;
};

using ValueFormatters = std::vector<ValueFormatter>;
using std::chrono::nanoseconds;
using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::minutes;
using std::chrono::hours;

template <typename D>
inline ValueFormatter fmt(D duration, int width, const std::string& suffix,
                          int64_t nanos_factor) {
  return ValueFormatter{nanoseconds{duration}.count(), width, suffix,
                        nanos_factor};
}

const ValueFormatters& GetTimeFormatters() {
  static constexpr int64_t NANOS = 1;
  static constexpr int64_t MICROS = 1000;
  static constexpr int64_t MILLIS = 1000 * MICROS;
  static constexpr int64_t SECS = 1000 * MILLIS;
  static constexpr int64_t MINS = 60 * SECS;
  static constexpr int64_t HOURS = 60 * MINS;
  static ValueFormatters time_fmt{fmt(nanoseconds{10}, 1, "ns", NANOS),
                                  fmt(nanoseconds{100}, 2, "ns", NANOS),
                                  fmt(microseconds{1}, 3, "ns", NANOS),
                                  fmt(microseconds{8}, 4, "ns", NANOS),
                                  fmt(microseconds{10}, 1, "us", MICROS),
                                  fmt(microseconds{100}, 2, "us", MICROS),
                                  fmt(milliseconds{1}, 3, "us", MICROS),
                                  fmt(milliseconds{8}, 4, "us", MICROS),
                                  fmt(milliseconds{10}, 1, "ms", MILLIS),
                                  fmt(milliseconds{100}, 2, "ms", MILLIS),
                                  fmt(seconds{1}, 3, "ms", MILLIS),
                                  fmt(seconds{8}, 4, "ms", MILLIS),
                                  fmt(seconds{10}, 1, "s", SECS),
                                  fmt(seconds{100}, 2, "s", SECS),
                                  fmt(minutes{8}, 3, "s", SECS),
                                  fmt(minutes{10}, 1, "min", MINS),
                                  fmt(minutes{100}, 2, "min", MINS),
                                  fmt(hours{8}, 3, "min", MINS),
                                  fmt(hours{10}, 1, "h", HOURS),
                                  fmt(hours{100}, 2, "h", HOURS),
                                  fmt(hours{24 * 8}, 3, "h", HOURS),
                                  fmt(hours::max(), 6, "h", HOURS)};
  return time_fmt;
}

ValueFormatter bin(int64_t max, int pow, int width, const char* suffix) {
  int shift = pow * 10;
  int64_t maxBytes = (shift == 0) ? max : max << shift;
  return ValueFormatter{maxBytes, width, suffix, 1 << shift};
}

ValueFormatters initBinaryFormatters() {
  std::array<const char*, 6> units{
      {"_B", "_KiB", "_MiB", "_GiB", "_TiB", "_PiB"}};
  ValueFormatters v;
  auto unitsLength = static_cast<int>(units.size());
  for (auto i = 0; i < unitsLength; ++i) {
    const auto unit = units.at(i);
    v.emplace_back(bin(10, i, 1, unit));
    v.emplace_back(bin(100, i, 2, unit));
    v.emplace_back(bin(1000, i, 3, unit));
    v.emplace_back(bin(10000, i, 4, unit));
  }
  v.emplace_back(ValueFormatter{std::numeric_limits<int64_t>::max(), 4, "_PiB",
                                int64_t{1} << 50});
  return v;
}

int64_t pow10(int64_t a, int b) {
  auto res = a;
  for (auto i = 0; i < b; ++i) {
    res *= 10;
  }
  return res;
}

ValueFormatter dec(int64_t max, int pow, int width, const char* suffix) {
  auto factor = pow10(1, pow);
  auto maxBytes = max * factor;
  return ValueFormatter{maxBytes, width, suffix, factor};
}

ValueFormatters initDecimalFormatters() {
  std::array<const char*, 6> units{{"", "_k", "_M", "_G", "_T", "_P"}};
  ValueFormatters v;
  auto unitsLength = static_cast<int>(units.size());
  for (auto i = 0; i < unitsLength; ++i) {
    const auto unit = units.at(i);
    auto pow = i * 3;
    v.emplace_back(dec(10, pow, 1, unit));
    v.emplace_back(dec(100, pow, 2, unit));
    v.emplace_back(dec(1000, pow, 3, unit));
  }
  v.emplace_back(ValueFormatter{std::numeric_limits<int64_t>::max(), 1, "_E",
                                pow10(1, 18)});
  return v;
}

const ValueFormatters& GetDecimalFormatters() {
  static ValueFormatters dec_fmt = initDecimalFormatters();
  return dec_fmt;
}

const ValueFormatters& GetBinaryFormatters() {
  static ValueFormatters bin_fmt = initBinaryFormatters();
  return bin_fmt;
}

const ValueFormatter& GetFormatter(const ValueFormatters& vfs, int64_t max) {
  for (const auto& vf : vfs) {
    if (max < vf.Max()) {
      return vf;
    }
  }
  return vfs[vfs.size() - 1];
}

std::function<std::string(int64_t)> BiasZero(const char* ltZero,
                                             const char* gtMax, int64_t max,
                                             const ValueFormatter& vf) {
  Buckets buckets;
  buckets.emplace_back(std::string{ltZero}, -1);
  buckets.emplace_back(vf.NewBucket(max / 8));
  buckets.emplace_back(vf.NewBucket(max / 4));
  buckets.emplace_back(vf.NewBucket(max / 2));
  buckets.emplace_back(vf.NewBucket(max));
  return BucketsFunction(std::move(buckets), gtMax);
}

std::function<std::string(int64_t)> BiasMax(const char* ltZero,
                                            const char* gtMax, int64_t max,
                                            const ValueFormatter& vf) {
  Buckets buckets;
  buckets.emplace_back(std::string{ltZero}, -1);
  buckets.emplace_back(vf.NewBucket(max - max / 2));
  buckets.emplace_back(vf.NewBucket(max - max / 4));
  buckets.emplace_back(vf.NewBucket(max - max / 8));
  buckets.emplace_back(vf.NewBucket(max));
  return BucketsFunction(std::move(buckets), gtMax);
}

std::function<std::string(int64_t)> TimeBiasZero(const char* ltZero,
                                                 const char* gtMax,
                                                 nanoseconds nanos) {
  const auto v = nanos.count();
  const auto& f = GetFormatter(GetTimeFormatters(), v);
  return BiasZero(ltZero, gtMax, v, f);
}

std::function<std::string(int64_t)> TimeBiasMax(const char* ltZero,
                                                const char* gtMax,
                                                nanoseconds nanos) {
  const auto v = nanos.count();
  const auto& f = GetFormatter(GetTimeFormatters(), v);
  return BiasMax(ltZero, gtMax, v, f);
}
}  // namespace

BucketFunction Age(nanoseconds duration) {
  return TimeBiasZero("future", "old", duration);
}

BucketFunction AgeBiasOld(nanoseconds duration) {
  return TimeBiasMax("future", "old", duration);
}

BucketFunction Latency(nanoseconds duration) {
  return TimeBiasZero("negative_latency", "slow", duration);
}

BucketFunction LatencyBiasSlow(nanoseconds duration) {
  return TimeBiasMax("negative_latency", "slow", duration);
}

BucketFunction Bytes(int64_t max) {
  const auto& f = GetFormatter(GetBinaryFormatters(), max);
  return BiasZero("negative", "large", max, f);
}

BucketFunction Decimal(int64_t max) {
  const auto& f = GetFormatter(GetDecimalFormatters(), max);
  return BiasZero("negative", "large", max, f);
}

}  // namespace bucket_functions
}  // namespace meter
}  // namespace atlas
