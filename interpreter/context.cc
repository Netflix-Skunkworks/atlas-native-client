#include "context.h"
#include "../util/config_manager.h"

namespace atlas {
namespace interpreter {

Context::Context() {}

static void ensure_stack_notempty(const Context::Stack& stack) {
  if (stack.empty()) {
    throw std::underflow_error(
        "Stack underflow: expecting a string on the stack.");
  }
}

std::shared_ptr<Expression> Context::PopExpression() {
  ensure_stack_notempty(stack_);
  auto top = stack_.at(stack_.size() - 1);
  stack_.pop_back();
  return top;
}

const std::shared_ptr<Expression>& Context::TopOfStack() {
  ensure_stack_notempty(stack_);
  return stack_.at(stack_.size() - 1);
}

static void ensure(bool b, const std::string& msg) {
  if (!b) {
    throw std::runtime_error(msg);
  }
}

std::string Context::PopString() {
  ensure_stack_notempty(stack_);
  auto maybe_str = PopExpression();
  ensure(expression::IsLiteral(*maybe_str),
         "Wrong type. Expecting a literal string.");
  return std::static_pointer_cast<Literal>(maybe_str)->AsString();
}

void Context::PushToList(std::shared_ptr<Expression> expression) {
  ensure_stack_notempty(stack_);
  auto top = TopOfStack().get();
  ensure(expression::IsList(*top), "Wrong type. Expecting a list.");
  auto list = static_cast<List*>(top);
  list->Add(expression);
}

size_t Context::StackSize() const noexcept { return stack_.size(); }

std::ostream& Context::Dump(std::ostream& os) const {
  os << "Context: {"
     << "\n";
  bool first = true;
  for (auto& elt : stack_) {
    if (!first) {
      os << ",\n";
    } else {
      first = false;
    }
    os << *elt;
  }
  os << "}\n";
  return os;
}

void Context::Push(const std::shared_ptr<Expression>& expression) {
  stack_.push_back(expression);
}

std::ostream& operator<<(std::ostream& os, const Context& context) {
  return context.Dump(os);
}
}  // namespace interpreter
}  // namespace atlas
