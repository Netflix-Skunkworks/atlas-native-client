#include "statistic.h"

namespace atlas {
namespace meter {

namespace statistic {
const Tag count = Tag::of("statistic", "count");
const Tag gauge = Tag::of("statistic", "gauge");
const Tag totalTime = Tag::of("statistic", "totalTime");
const Tag totalAmount = Tag::of("statistic", "totalAmount");
const Tag max = Tag::of("statistic", "max");
const Tag totalOfSquares = Tag::of("statistic", "totalOfSquares");
const Tag duration = Tag::of("statistic", "duration");
const Tag activeTasks = Tag::of("statistic", "activeTasks");
const Tag percentile = Tag::of("statistic", "percentile");
}  // namespace statistic
}  // namespace meter
}  // namespace atlas
