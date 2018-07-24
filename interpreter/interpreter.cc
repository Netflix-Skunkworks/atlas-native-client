#include "interpreter.h"
#include "../util/logger.h"
#include "../util/optional.h"
#include "group_by.h"

using atlas::util::Logger;

namespace atlas {
namespace interpreter {

inline void add_nonempty(const std::string::size_type i,
                         const std::string::size_type j, const std::string& s,
                         Expressions* result) {
  // j starts pointing at the comma or end of string
  auto k = static_cast<int>(j) - 1;
  // trim right
  while (k >= 0 && static_cast<bool>(std::isspace(s[k]))) {
    --k;
  }
  // point to the last char that's not a space
  ++k;

  // only non-empty tokens
  if (k > static_cast<int>(i)) {
    result->push_back(std::make_unique<Literal>(s.substr(i, k - i)));
  }
}

// split on , and trim the elements. Does not add empty elements to the result
void split(const std::string& s, Expressions* result) {
  auto i = std::string::size_type(0);

  // trim left
  while (static_cast<bool>(isspace(s[i]))) {
    ++i;
  }
  auto j = s.find(',', i);

  while (j != std::string::npos) {
    add_nonempty(i, j, s, result);
    i = j + 1;
    // trim left
    while (static_cast<bool>(isspace(s[i]))) {
      ++i;
    }
    j = s.find(',', i);
  }
  add_nonempty(i, s.length(), s, result);
}

Interpreter::Interpreter(std::unique_ptr<Vocabulary> vocabulary)
    : vocabulary_(std::move(vocabulary)) {}

static const char* UNBALANCED = "Unbalanced parenthesis";

static OptionalString do_program(Context* context, const Vocabulary& vocabulary,
                                 Expressions* tokens, int depth) {
  if (tokens->empty()) {
    if (depth == 0) {
      return kNone;
    }
    return OptionalString{UNBALANCED};
  }

  int list_depth = depth;
  for (auto& token : *tokens) {
    if (expression::Is(*token, "(")) {
      ++list_depth;
      if (list_depth == 1) {
        context->Push(std::make_unique<List>());
      } else {
        context->PushToList(std::move(token));
      }
    } else if (expression::Is(*token, ")")) {
      --list_depth;
      if (list_depth > 0) {
        context->PushToList(std::move(token));
      } else if (list_depth < 0) {
        return OptionalString{UNBALANCED};
      }
    } else if (expression::IsWord(*token)) {
      const std::string& word = expression::GetWord(*token);
      if (list_depth == 0) {
        vocabulary.Execute(context, word);
      } else {
        context->PushToList(std::move(token));
      }
    } else {
      if (list_depth == 0) {
        context->Push(std::move(token));
      } else {
        context->PushToList(std::move(token));
      }
    }
  }
  return kNone;
}

void Interpreter::Execute(Context* context, const std::string& program) const {
  Expressions tokens;
  split(program, &tokens);
  do_program(context, *vocabulary_, &tokens, 0);
}

std::shared_ptr<Query> Interpreter::GetQuery(const std::string& program) const {
  Context context;
  Execute(&context, program);
  if (context.StackSize() != 1) {
    Logger()->error("Failed to get query from '{}': {} expressions", program,
                    context.StackSize());
    return query::false_q();
  }

  auto top = context.PopExpression();
  switch (top->GetType()) {
    case ExpressionType::Query:
      return std::static_pointer_cast<Query>(top);
    case ExpressionType::ValueExpression:
      return static_cast<ValueExpression&>(*top).GetQuery();
    case ExpressionType::MultipleResults:
      return static_cast<MultipleResults&>(*top).GetQuery();
    default:
      Logger()->error(
          "Invalid expression on stack. Expecting a query, value-expression, "
          "or group-by");
      return query::false_q();
  }
}

}  // namespace interpreter
}  // namespace atlas
