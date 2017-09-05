#pragma once

#include "../types.h"
#include "context.h"
#include "word.h"

namespace atlas {
namespace interpreter {
class Vocabulary {
 public:
  virtual OptionalString Execute(Context* context,
                                 const std::string& token) const = 0;
  virtual ~Vocabulary() = default;
};

class ClientVocabulary : public Vocabulary {
 public:
  ClientVocabulary();
  virtual ~ClientVocabulary() = default;

  OptionalString Execute(Context* context,
                         const std::string& token) const override;

 private:
  std::map<std::string, std::unique_ptr<Word>> words;
};
}  // namespace interpreter
}  // namespace atlas
