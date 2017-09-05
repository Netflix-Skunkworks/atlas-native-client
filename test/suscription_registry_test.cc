#include "../interpreter/interpreter.h"
#include "../meter/manual_clock.h"
#include "../meter/subscription_registry.h"
#include "../util/config_manager.h"
#include <gtest/gtest.h>

using atlas::util::Config;
using atlas::util::ConfigManager;
using atlas::util::DefaultConfig;
using atlas::interpreter::TagsValuePair;
using atlas::interpreter::TagsValuePairs;
using atlas::interpreter::Interpreter;
using atlas::interpreter::ClientVocabulary;
using namespace atlas::meter;

class SR : public SubscriptionRegistry {
 public:
  SR()
      : SubscriptionRegistry(std::make_unique<Interpreter>(
            std::make_unique<ClientVocabulary>())),
        clock_() {}
  virtual ~SR() {}

  const Clock& clock() const noexcept override { return clock_; }

  TagsValuePairs eval(const std::string& expression,
                      const TagsValuePairs& measurements) const {
    return SubscriptionRegistry::evaluate(expression, measurements);
  }

 private:
  ManualClock clock_;
};

TEST(SubscriptionRegistry, Eval) {
  SR registry;
  const auto& manual_clock = static_cast<const ManualClock&>(registry.clock());
  manual_clock.SetWall(42);

  const std::string s{":true,:all"};

  Tags tags1{{"name", "m1"}, {"k1", "v1"}, {"k1", "v2"}};
  Tags tags2{{"name", "m2"}, {"k1", "v1"}, {"k1", "v2"}};
  Tags tags3{{"name", "m3"}, {"k1", "v1"}, {"k1", "v2"}};
  TagsValuePair m1{tags1, 1.0};
  TagsValuePair m2{tags2, 2.0};
  TagsValuePair m3{tags3, 3.0};
  TagsValuePairs measurements{m1, m2, m3};
  const auto& res = registry.eval(s, measurements);

  EXPECT_EQ(res.size(), 3);
}

TEST(SubscriptionRegistry, MainMetrics) {
  SR registry;
  const auto& manual_clock = static_cast<const ManualClock&>(registry.clock());
  manual_clock.SetWall(42);
  Tags tags{{"k1", "v1"}, {"k1", "v2"}};
  auto id1 = registry.CreateId("m1", tags);
  auto id3 = registry.CreateId("m3", tags);

  registry.counter(id1)->Increment();
  // make sure we re-use ids
  registry.counter(registry.CreateId("m2", tags))->Add(60);
  registry.counter(registry.CreateId("m2", tags))->Add(60);
  registry.counter(id3)->Add(60);

  const auto& cfg = DefaultConfig();
  manual_clock.SetWall(60042);
  const auto& res = registry.GetMainMeasurements(*cfg);
  EXPECT_EQ(res.size(), 3);
}
