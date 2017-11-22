#include "../interpreter/interpreter.h"
#include "../meter/manual_clock.h"
#include "../meter/subscription_registry.h"
#include "../util/config_manager.h"
#include "../util/logger.h"
#include <gtest/gtest.h>

using atlas::util::Config;
using atlas::util::ConfigManager;
using atlas::util::DefaultConfig;
using atlas::util::intern_str;
using atlas::util::Logger;

using atlas::interpreter::kNameRef;
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

  std::shared_ptr<TagsValuePairs> eval(
      const std::string& expression,
      std::shared_ptr<TagsValuePairs> measurements) const {
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
  auto m1 = TagsValuePair::of(std::move(tags1), 1.0);
  auto m2 = TagsValuePair::of(std::move(tags2), 2.0);
  auto m3 = TagsValuePair::of(std::move(tags3), 3.0);
  auto measurements = std::make_shared<TagsValuePairs>();
  measurements->push_back(std::move(m1));
  measurements->push_back(std::move(m2));
  measurements->push_back(std::move(m3));
  const auto& res = registry.eval(s, measurements);
  EXPECT_EQ(res->size(), 3);
}

TEST(SubscriptionRegistry, MainMetrics) {
  SR registry;
  const auto& manual_clock = static_cast<const ManualClock&>(registry.clock());
  manual_clock.SetWall(42);
  Tags tags{{"k1", "v1"}, {"k2", "v2"}};
  auto id1 = registry.CreateId("m1", tags);
  auto id3 = registry.CreateId("m3", tags);
  Tags tags_for_4{{"k1", "v1"}, {"k2", "v3"}};
  auto id4 = registry.CreateId("m3", tags_for_4);

  registry.counter(id1)->Increment();
  // make sure we re-use ids
  registry.counter(registry.CreateId("m2", tags))->Add(60);
  registry.counter(registry.CreateId("m2", tags))->Add(60);
  registry.counter(id3)->Add(60);
  registry.counter(id4)->Add(60);

  auto pub_config = std::vector<std::string>{
      "name,m1,:eq,:all", ":true,(,k2,nf.node,nf.region,),:drop-tags"};
  auto cfg = DefaultConfig()->WithPublishConfig(pub_config);
  manual_clock.SetWall(60042);
  auto common_tags = cfg.CommonTags();
  const auto& res = registry.GetMainMeasurements(cfg, common_tags);
  EXPECT_EQ(res->size(), 3);

  auto name_num_tags = std::unordered_map<std::string, size_t>(res->size());
  auto name_value = std::unordered_map<std::string, double>(res->size());

  for (const auto& m : *res) {
    const auto& t = m->all_tags();
    const auto& name = t.at(kNameRef).get();
    name_num_tags[name] = t.size();
    name_value[name] = m->value();
  }

  auto ct = common_tags.size();
  auto expected_name_num_tags = std::unordered_map<std::string, size_t>(
      {{"m1", 4u + ct},
       {"m2", 3u + ct - 2},
       {"m3", 3u + ct - 2}});
  EXPECT_EQ(name_num_tags, expected_name_num_tags);

  auto expected_name_value = std::unordered_map<std::string, double>(
      {{"m1", 1/60.0},
       {"m2", 120/60.0},
       {"m3", 120/60.0}});
  EXPECT_EQ(name_value, expected_name_value);
}
