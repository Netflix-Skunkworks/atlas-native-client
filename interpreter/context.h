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

  explicit Context(std::unique_ptr<Stack> stack);

  std::string PopString();

  std::shared_ptr<Expression> PopExpression();

  void Push(std::shared_ptr<Expression> expression);

  void PushToList(std::shared_ptr<Expression> expression);

  std::size_t StackSize() const noexcept;

  std::ostream& Dump(std::ostream& os) const;

 private:
  std::unique_ptr<Stack> stack_;
  const std::shared_ptr<Expression>& TopOfStack();
};

std::ostream& operator<<(std::ostream& os, const Context& context);
}  // namespace interpreter
}  // namespace atlas
