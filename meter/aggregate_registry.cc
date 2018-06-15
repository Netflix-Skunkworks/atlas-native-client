#include "aggregate_registry.h"
#include "../util/logger.h"
#include "id_format.h"

using atlas::util::Logger;

namespace atlas {
namespace meter {

enum class AggrOp { Add, Max };

static AggrOp op_for_statistic(util::StrRef stat) {
  static std::array<util::StrRef, 5> counters{
      util::intern_str("count"), util::intern_str("totalAmount"),
      util::intern_str("totalTime"), util::intern_str("totalOfSquares"),
      util::intern_str("percentile")};

  auto c = std::find(counters.begin(), counters.end(), stat);
  return c == counters.end() ? AggrOp::Max : AggrOp::Add;
}

static AggrOp aggregate_op(const Measurement& measurement) {
  static util::StrRef statistic = util::intern_str("statistic");
  const auto& tags = measurement.id->GetTags();
  auto stat = tags.at(statistic);
  return op_for_statistic(stat);
}

static double addnan(double a, double b) {
  if (std::isnan(a)) {
    return b;
  }
  if (std::isnan(b)) {
    return a;
  }
  return a + b;
}

static double maxnan(double a, double b) {
  if (std::isnan(a)) {
    return b;
  }
  if (std::isnan(b)) {
    return a;
  }
  return std::max(a, b);
}

AggregateRegistry::AggregateRegistry(int64_t update_frequency,
                                     int64_t reporting_frequency) noexcept
    : update_multiple{reporting_frequency / update_frequency} {
  assert(reporting_frequency % update_frequency == 0);
  assert(update_frequency % 1000 == 0);
}

void AggregateRegistry::update_from(const Measurements& measurements) noexcept {
  std::lock_guard<std::mutex> guard{mutex};

  double new_value;
  for (const auto& m : measurements) {
    auto& my_m = my_measures[m.id];
    my_m.id = m.id;
    my_m.timestamp = m.timestamp;
    auto op = aggregate_op(m);
    switch (op) {
      case AggrOp::Add:
        new_value = addnan(my_m.value, m.value / update_multiple);
        my_m.value = new_value;
        break;
      case AggrOp::Max:
        new_value = maxnan(m.value, my_m.value);
        my_m.value = new_value;
        break;
    }
  }
}

Measurements AggregateRegistry::measurements() const noexcept {
  std::lock_guard<std::mutex> guard{mutex};

  Measurements result;
  result.reserve(my_measures.size());
  for (const auto& m : my_measures) {
    result.push_back(m.second);
  }
  Logger()->debug("Returning {} measurements, and resetting aggregate registry",
                  result.size());
  my_measures.clear();
  return result;
}

}  // namespace meter
}  // namespace atlas
