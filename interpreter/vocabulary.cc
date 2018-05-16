#include "vocabulary.h"
#include "aggregation.h"
#include "all.h"
#include "group_by.h"
#include "keep_drop_tags.h"
#include <sstream>

namespace atlas {
namespace interpreter {

namespace {
// query words
class HasKeyWord : public Word {
 public:
  OptionalString Execute(Context* context) override {
    const std::string& k = context->PopString();
    auto expression = std::make_unique<HasKeyQuery>(k);
    context->Push(std::move(expression));
    return kNone;
  }
};

OptionalString do_query(Context* context, RelOp op) {
  try {
    const std::string v = context->PopString();
    const std::string k = context->PopString();
    auto expression = std::make_unique<RelopQuery>(k, v, op);
    context->Push(std::move(expression));
    return kNone;
  } catch (const std::exception& e) {
    return OptionalString(e.what());
  }
}

class EqualWord : public Word {
 public:
  OptionalString Execute(Context* context) override {
    return do_query(context, RelOp::EQ);
  }
};

class GtWord : public Word {
 public:
  OptionalString Execute(Context* context) override {
    return do_query(context, RelOp::GT);
  }
};

class GeWord : public Word {
 public:
  OptionalString Execute(Context* context) override {
    return do_query(context, RelOp::GE);
  }
};

class LtWord : public Word {
 public:
  OptionalString Execute(Context* context) override {
    return do_query(context, RelOp::LT);
  }
};

class LeWord : public Word {
 public:
  OptionalString Execute(Context* context) override {
    return do_query(context, RelOp::LE);
  }
};

class InWord : public Word {
 public:
  OptionalString Execute(Context* context) override {
    auto list_expr = context->PopExpression();

    if (list_expr->GetType() != ExpressionType::List) {
      return OptionalString(":in expects a list on the stack");
    }

    auto list = std::static_pointer_cast<List>(list_expr);
    auto key = context->PopString();
    auto in_expr = std::make_unique<InQuery>(key, list->ToStrings());
    context->Push(std::move(in_expr));
    return kNone;
  }
};

class RegexWord : public Word {
 public:
  explicit RegexWord(bool ignore_case) : ignore_case_(ignore_case) {}

  OptionalString Execute(Context* context) override {
    const auto v = context->PopString();
    const auto k = context->PopString();
    auto expression = std::make_unique<RegexQuery>(k, v, ignore_case_);

    context->Push(std::move(expression));
    return kNone;
  }

 private:
  bool ignore_case_;
};

class NotWord : public Word {
 public:
  OptionalString Execute(Context* context) override {
    auto expr = context->PopExpression();
    if (expression::IsQuery(*expr)) {
      auto query = std::static_pointer_cast<Query>(expr);
      context->Push(query::not_q(query));
    } else {
      return OptionalString(":not expects a query expression on the stack");
    }

    return kNone;
  }
};

class AndWord : public Word {
 public:
  OptionalString Execute(Context* context) override {
    auto e2 = context->PopExpression();
    auto e1 = context->PopExpression();

    if (expression::IsQuery(*e1) && expression::IsQuery(*e2)) {
      auto q1 = std::static_pointer_cast<Query>(e1);
      auto q2 = std::static_pointer_cast<Query>(e2);
      context->Push(query::and_q(q1, q2));
      return kNone;
    }

    return OptionalString(":and expects two queries on the stack");
  }
};

class OrWord : public Word {
 public:
  OptionalString Execute(Context* context) override {
    auto e2 = context->PopExpression();
    auto e1 = context->PopExpression();

    if (expression::IsQuery(*e1) && expression::IsQuery(*e2)) {
      auto q1 = std::static_pointer_cast<Query>(e1);
      auto q2 = std::static_pointer_cast<Query>(e2);
      context->Push(query::or_q(q1, q2));
      return kNone;
    }
    return OptionalString(":or expects two queries or a query and a constant");
  }
};

class FalseWord : public Word {
 public:
  OptionalString Execute(Context* context) override {
    context->Push(query::false_q());
    return kNone;
  }
};

class TrueWord : public Word {
 public:
  OptionalString Execute(Context* context) override {
    context->Push(query::true_q());
    return kNone;
  }
};

// aggregation
class CountWord : public Word {
 public:
  OptionalString Execute(Context* context) override {
    const auto& e = context->PopExpression();
    if (expression::IsQuery(*e)) {
      auto query = std::static_pointer_cast<Query>(e);
      context->Push(aggregation::count(query));
    } else {
      return OptionalString(":count was expecting a query on the stack");
    }
    return kNone;
  }
};

class SumWord : public Word {
 public:
  OptionalString Execute(Context* context) override {
    const auto& e = context->PopExpression();
    if (expression::IsQuery(*e)) {
      auto query = std::static_pointer_cast<Query>(e);
      context->Push(aggregation::sum(query));
    } else {
      return OptionalString(":sum was expecting a query on the stack");
    }
    return kNone;
  }
};

class MinWord : public Word {
 public:
  OptionalString Execute(Context* context) override {
    const auto& e = context->PopExpression();
    if (expression::IsQuery(*e)) {
      auto query = std::static_pointer_cast<Query>(e);
      context->Push(aggregation::min(query));
    } else {
      return OptionalString(":min was expecting a query on the stack");
    }
    return kNone;
  }
};

class MaxWord : public Word {
 public:
  OptionalString Execute(Context* context) override {
    const auto& e = context->PopExpression();
    if (expression::IsQuery(*e)) {
      auto query = std::static_pointer_cast<Query>(e);
      context->Push(aggregation::max(query));
    } else {
      return OptionalString(":max was expecting a query on the stack");
    }
    return kNone;
  }
};

class AvgWord : public Word {
 public:
  OptionalString Execute(Context* context) override {
    const auto& e = context->PopExpression();
    if (expression::IsQuery(*e)) {
      auto query = std::static_pointer_cast<Query>(e);
      context->Push(aggregation::avg(query));
    } else {
      return OptionalString(":avg was expecting a query on the stack");
    }
    return kNone;
  }
};

std::shared_ptr<ValueExpression> value_expr_from(
    const std::shared_ptr<Expression>& e) {
  // a constant
  if (expression::IsLiteral(*e)) {
    auto n = std::stod(expression::LiteralToStr(*e));
    return std::make_shared<ConstantExpression>(n);
  }

  // if it's a query aggregate it using the default :sum
  if (expression::IsQuery(*e)) {
    auto query = std::static_pointer_cast<Query>(e);
    return aggregation::sum(query);
  }

  if (e->GetType() == ExpressionType::ValueExpression) {
    return std::static_pointer_cast<ValueExpression>(e);
  }

  return nullptr;
}

class AllWord : public Word {
 public:
  OptionalString Execute(Context* context) override {
    auto q = context->PopExpression();
    if (!expression::IsQuery(*q)) {
      return OptionalString(":all was expecting a query on the stack");
    }

    context->Push(std::make_shared<All>(std::static_pointer_cast<Query>(q)));
    return kNone;
  }
};

class DropKeepTagsWord : public Word {
 public:
  explicit DropKeepTagsWord(bool keep) : keep_(keep) {}

  OptionalString Execute(Context* context) override {
    auto k = context->PopExpression();
    bool error = false;
    if (!expression::IsList(*k)) {
      error = true;
    }
    auto value_expr = value_expr_from(context->PopExpression());
    if (!value_expr) {
      error = true;
    }

    if (!error) {
      auto keys = std::static_pointer_cast<List>(k);
      context->Push(std::make_shared<KeepOrDropTags>(*keys, value_expr, keep_));
      return kNone;
    }

    auto msg = fmt::format(
        "{}-tags was expecting a list and a data expression or "
        "query on the stack",
        keep_ ? ":keep" : ":drop");
    return OptionalString{msg};
  }

 private:
  bool keep_;
};

class GroupByWord : public Word {
 public:
  OptionalString Execute(Context* context) override {
    auto k = context->PopExpression();
    bool error = false;
    if (!expression::IsList(*k)) {
      error = true;
    }
    auto value_expr = value_expr_from(context->PopExpression());
    if (!value_expr) {
      error = true;
    }

    if (!error) {
      auto keys = std::static_pointer_cast<List>(k);
      context->Push(std::make_shared<GroupBy>(*keys, value_expr));
      return kNone;
    }
    return OptionalString(
        ":by was expecting a list and a data expression or "
        "query on the stack");
  }
};
}  // namespace

ClientVocabulary::ClientVocabulary() {
  // query words
  words["has"] = std::make_unique<HasKeyWord>();
  words["eq"] = std::make_unique<EqualWord>();
  words["gt"] = std::make_unique<GtWord>();
  words["ge"] = std::make_unique<GeWord>();
  words["lt"] = std::make_unique<LtWord>();
  words["le"] = std::make_unique<LeWord>();
  words["in"] = std::make_unique<InWord>();
  words["re"] = std::make_unique<RegexWord>(false);
  words["reic"] = std::make_unique<RegexWord>(true);
  words["not"] = std::make_unique<NotWord>();
  words["and"] = std::make_unique<AndWord>();
  words["or"] = std::make_unique<OrWord>();
  words["false"] = std::make_unique<FalseWord>();
  words["true"] = std::make_unique<TrueWord>();
  // aggregation words
  words["count"] = std::make_unique<CountWord>();
  words["sum"] = std::make_unique<SumWord>();
  words["min"] = std::make_unique<MinWord>();
  words["max"] = std::make_unique<MaxWord>();
  words["avg"] = std::make_unique<AvgWord>();
  // by
  words["by"] = std::make_unique<GroupByWord>();
  words["keep-tags"] = std::make_unique<DropKeepTagsWord>(true);
  words["drop-tags"] = std::make_unique<DropKeepTagsWord>(false);
  words["all"] = std::make_unique<AllWord>();
}

OptionalString ClientVocabulary::Execute(Context* context,
                                         const std::string& token) const {
  auto w = words.find(token);
  if (w == words.end()) {
    return OptionalString{"Unknown word " + token};
  }
  return w->second->Execute(context);
}
}  // namespace interpreter
}  // namespace atlas
