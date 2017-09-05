#include "clock.h"
#include <chrono>

namespace atlas {
namespace meter {

using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::nanoseconds;
using std::chrono::steady_clock;
using std::chrono::system_clock;

int64_t SystemClock::WallTime() const noexcept {
  return static_cast<int64_t>(
      duration_cast<milliseconds>(system_clock::now().time_since_epoch())
          .count());
}

int64_t SystemClock::MonotonicTime() const noexcept {
  return static_cast<int64_t>(
      duration_cast<nanoseconds>(steady_clock::now().time_since_epoch())
          .count());
}
}  // namespace meter
}  // namespace atlas
