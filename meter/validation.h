#pragma once

#include "id.h"

namespace atlas {
namespace meter {
namespace validation {

/// Return whether the given tags will pass validation rules done by our
/// backends. It assumes we will fix invalid characters
bool IsValid(const Tags& tags) noexcept;

}  // namespace validation
}  // namespace meter
}  // namespace atlas
