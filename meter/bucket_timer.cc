#include "bucket_timer.h"
namespace atlas {
namespace meter {

BucketTimer::BucketTimer(Registry* registry, IdPtr id,
                         BucketFunction bucket_function)
    : Meter{id, registry->clock()},
      registry_{registry},
      bucket_function_{std::move(bucket_function)} {}

std::ostream& BucketTimer::Dump(std::ostream& os) const {
  os << "BucketTimer{" << *id_ << "}";
  return os;
}

static const Measurements kEmptyMeasurements;
Measurements BucketTimer::MeasuresForPoller(size_t /*poller_idx*/) const {
  return kEmptyMeasurements;
}

static const std::string kBucket{"bucket"};

void BucketTimer::Record(std::chrono::nanoseconds duration) {
  const auto& bucket = bucket_function_(duration.count());
  registry_->timer(id_->WithTag(Tag::of(kBucket, bucket)))->Record(duration);
  Updated();
}

// not supported
int64_t BucketTimer::Count() const noexcept { return 0; }

// not supported
int64_t BucketTimer::TotalTime() const noexcept { return 0; }

}  // namespace meter
}  // namespace atlas
