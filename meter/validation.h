#pragma once

#include <set>
#include "id.h"

namespace atlas {
namespace meter {

/// Return whether the given tags will pass validation rules done by our
/// backends. It assumes we will fix invalid characters
bool AreTagsValid(const Tags& tags) noexcept;

struct ValidationIssue {
  enum class Level { WARN, ERROR };
  Level level;
  std::string description;

  bool operator<(const ValidationIssue& rhs) const noexcept;
  std::string ToString() const noexcept;
  static ValidationIssue Err(std::string message) noexcept;
  static ValidationIssue Warn(std::string message) noexcept;
};

using ValidationIssues = std::set<ValidationIssue>;
ValidationIssues AnalyzeTags(const Tags& tags) noexcept;

}  // namespace meter
}  // namespace atlas
