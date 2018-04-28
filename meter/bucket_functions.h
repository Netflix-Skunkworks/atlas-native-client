#pragma once
#include <chrono>
#include <functional>
#include <string>

namespace atlas {
namespace meter {

using BucketFunction = std::function<std::string(int64_t)>;
namespace bucket_functions {

///
/// Returns a function that maps age values to a set of buckets. Example
/// use-case would be
/// tracking the age of data flowing through a processing pipeline. Values that
/// are less than
/// 0 will be marked as "future". These typically occur due to minor variations
/// in the clocks
/// across nodes. In addition to a bucket at the max, it will create buckets at
/// max / 2, max / 4,
/// and max / 8.
///
/// @param duration
///     Maximum expected age of data flowing through. Values greater than this
///     max will be mapped
///     to an "old" bucket.
/// @return
///     Function mapping age values to string labels. The labels for buckets
///     will sort
///     so they can be used with a simple group by.
///
BucketFunction Age(std::chrono::nanoseconds duration);

///
/// Returns a function that maps age values to a set of buckets. Example
/// use-case would be
/// tracking the age of data flowing through a processing pipeline. Values that
/// are less than
/// 0 will be marked as "future". These typically occur due to minor variations
/// in the clocks
/// across nodes. In addition to a bucket at the max, it will create buckets at
/// max - max / 8,
/// max - max / 4, and max - max / 2.
///
/// @param max
///     Maximum expected age of data flowing through. Values greater than this
///     max will be mapped
///     to an "old" bucket.
/// @param unit
///     Unit for the max value.
/// @return
///     Function mapping age values to string labels. The labels for buckets
///     will sort
///     so they can be used with a simple group by.
///
BucketFunction AgeBiasOld(std::chrono::nanoseconds duration);

///
/// Returns a function that maps latencies to a set of buckets. Example use-case
/// would be
/// tracking the amount of time to process a request on a server. Values that
/// are less than
/// 0 will be marked as "negative_latency". These typically occur due to minor
/// variations in the
/// clocks to measure the latency and having a
/// time adjustment between the start and end. In addition to a bucket at the
/// max, it will create
/// buckets at max / 2, max / 4, and max / 8.
///
/// @param duration
///     Maximum expected age of data flowing through. Values greater than this
///     max will be mapped
///     to an "old" bucket.
/// @return
///     Function mapping age values to string labels. The labels for buckets
///     will sort
///     so they can be used with a simple group by.
///
BucketFunction Latency(std::chrono::nanoseconds duration);

///
/// Returns a function that maps latencies to a set of buckets. Example use-case
/// would be
/// tracking the amount of time to process a request on a server. Values that
/// are less than
/// 0 will be marked as "negative_latency". These typically occur due to minor
/// variations in the
/// clocks to measure the latency and having a
/// time adjustment between the start and end. In addition to a bucket at the
/// max, it will create
/// buckets at max - max / 8, max - max / 4, and max - max / 2.
///
/// @param duration
///     Maximum expected age of data flowing through. Values greater than this
///     max will be mapped
///     to an "old" bucket.
/// @return
///     Function mapping age values to string labels. The labels for buckets
///     will sort
///     so they can be used with a simple group by.
///
BucketFunction LatencyBiasSlow(std::chrono::nanoseconds duration);

///
/// Returns a function that maps size values in bytes to a set of buckets. The
/// buckets will
/// use <a href="https://en.wikipedia.org/wiki/Binary_prefix">binary
/// prefixes</a> representing
/// powers of 1024. If you want powers of 1000, then see {@link #decimal(long)}.
///
/// @param max
///     Maximum expected size of data being recorded. Values greater than this
///     amount will be
///     mapped to a "large" bucket.
/// @return
///     Function mapping size values to string labels. The labels for buckets
///     will sort
///     so they can be used with a simple group by.
///
BucketFunction Bytes(int64_t max);

///
/// Returns a function that maps size values to a set of buckets. The buckets
/// will
/// use <a href="https://en.wikipedia.org/wiki/Metric_prefix">decimal
/// prefixes</a> representing
/// powers of 1000. If you are measuring quantities in bytes and want powers of
/// 1024, then see
/// {@link #bytes(long)}.
///
/// @param max
///     Maximum expected size of data being recorded. Values greater than this
///     amount will be
///     mapped to a "large" bucket.
/// @return
///     Function mapping size values to string labels. The labels for buckets
///     will sort
///     so they can be used with a simple group by.
///
BucketFunction Decimal(int64_t max);
}  // namespace bucket_functions
}  // namespace meter
}  // namespace atlas