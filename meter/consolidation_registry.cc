#include "consolidation_registry.h"
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

ConsolidationRegistry::ConsolidationRegistry(int64_t update_frequency,
                                             int64_t reporting_frequency,
                                             const Clock* clock) noexcept
    : reporting_frequency_{reporting_frequency},
      update_multiple{reporting_frequency / update_frequency},
      clock_{clock} {
  assert(reporting_frequency % update_frequency == 0);
  assert(update_frequency % 1000 == 0);
}

void ConsolidationRegistry::update_from(
    const Measurements& measurements) noexcept {
  std::lock_guard<std::mutex> guard{mutex};

  for (const auto& m : measurements) {
    if (std::isnan(m.value)) {
      continue;
    }

    auto it = my_values.find(m.id);
    auto op = aggregate_op(m);
    if (it == my_values.end()) {
      it = my_values
               .emplace(values_map::value_type{
                   m.id, ConsolidatedValue(op == AggrOp::Max, update_multiple,
                                           reporting_frequency_, clock_)})
               .first;
    }
    auto& my_value = it->second;
    my_value.update(m.value);
  }
}

Measurements ConsolidationRegistry::measurements(int64_t timestamp) const
    noexcept {
  std::lock_guard<std::mutex> guard{mutex};

  std::vector<IdPtr> to_remove;
  Measurements result;
  result.reserve(my_values.size());
  for (auto& m : my_values) {
    if (m.second.has_value()) {
      m.second.set_marked(false);
      result.emplace_back(Measurement{m.first, timestamp, m.second.value()});
    } else {
      // check whether we should expire them from our map
      // if it didn't have any activity during this interval, we mark them
      // if it was already marked, it means two periods with no activity, then
      // then we consider the entry safe to be removed
      if (m.second.marked()) {
        to_remove.emplace_back(m.first);
      } else {
        m.second.set_marked(true);
      }
    }
  }
  Logger()->debug("Returning {} measurements - Expiring {} entries",
                  result.size(), to_remove.size());
  for (const auto& id : to_remove) {
    my_values.erase(id);
  }
  return result;
}

}  // namespace meter
}  // namespace atlas
