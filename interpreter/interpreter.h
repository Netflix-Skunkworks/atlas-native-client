#pragma once

#include "context.h"
#include "expression.h"
#include "vocabulary.h"

namespace atlas {
namespace interpreter {
class Interpreter {
 public:
  explicit Interpreter(std::unique_ptr<Vocabulary> vocabulary);

  void Execute(Context* context, const std::string& program) const;

  std::shared_ptr<Query> GetQuery(const std::string& program) const;

 private:
  const std::unique_ptr<Vocabulary> vocabulary_;
};

using Expressions = std::vector<std::unique_ptr<Expression>>;

void split(const std::string& s, Expressions* result);

}  // namespace interpreter
}  // namespace atlas
