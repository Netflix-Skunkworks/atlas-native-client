#pragma once

#include <memory>
#include <stack>
#include "expression.h"
#include "query.h"

namespace atlas {
namespace interpreter {
class Context {
 public:
  using Stack = std::vector<std::shared_ptr<Expression>>;

  Context();

  util::StrRef PopString();

  std::shared_ptr<Expression> PopExpression();

  void Push(const std::shared_ptr<Expression>& expression);

  void PushToList(std::shared_ptr<Expression> expression);

  std::size_t StackSize() const noexcept;

  std::ostream& Dump(std::ostream& os) const;

 private:
  Stack stack_;
  const std::shared_ptr<Expression>& TopOfStack();
};

std::ostream& operator<<(std::ostream& os, const Context& context);
}  // namespace interpreter
}  // namespace atlas
