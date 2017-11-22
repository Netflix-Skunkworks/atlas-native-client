#pragma once

#include "../util/optional.h"

namespace atlas {
namespace interpreter {

class Word {
 public:
  virtual OptionalString Execute(Context* context) = 0;
  virtual ~Word() = default;
};
}
}
