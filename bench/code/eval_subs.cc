#include <rapidjson/document.h>
#include <benchmark/benchmark.h>
#include "../../interpreter/evaluator.h"
#include "../../interpreter/tags_value.h"
#include "../../meter/subscription.h"
#include "../../util/logger.h"

using atlas::interpreter::Evaluator;
using atlas::interpreter::IdTagsValuePair;
using atlas::interpreter::TagsValuePairs;
using atlas::meter::Id;
using atlas::meter::Subscription;
using atlas::meter::Subscriptions;
using atlas::meter::Tags;
using atlas::util::Logger;

namespace atlas {
namespace meter {

SubscriptionResults generate_sub_results(const Evaluator& evaluator,
                                         const Subscriptions& subs,
                                         const TagsValuePairs& pairs);
}
}  // namespace atlas

using atlas::meter::generate_sub_results;

char* read_file(const char* fname) {
  auto fp = fopen(fname, "r");
  if (fp == nullptr) {
    Logger()->error("{} is not readable: {}", fname, strerror(errno));
    return nullptr;
  }
  fseek(fp, 0, SEEK_END);
  auto filesize = static_cast<size_t>(ftell(fp));
  fseek(fp, 0, SEEK_SET);
  auto buffer = static_cast<char*>(malloc(filesize + 1));
  auto readLength = fread(buffer, 1, filesize, fp);
  buffer[readLength] = '\0';
  fclose(fp);
  return buffer;
}

static Subscriptions load_subs(char* buffer) {
  Subscriptions result;
  rapidjson::Document d;
  d.Parse(buffer);
  free(buffer);

  auto logger = atlas::util::Logger();
  if (d.IsObject()) {
    auto expressions = d["expressions"].GetArray();
    for (auto& e : expressions) {
      if (e.IsObject()) {
        auto expr = e.GetObject();
        auto freq = expr["frequency"].GetInt64();
        result.emplace_back(Subscription{expr["id"].GetString(), freq,
                                         expr["expression"].GetString()});
        if (result.size() > 3000) {
          break;
        }
      } else {
        logger->error("expr not an object!");
      }
    }
  } else {
    logger->error("subs: Not an object!");
  }

  logger->info("Loaded {} subs", result.size());
  return result;
}

static Tags& get_common_tags() {
  static Tags common_tags;
  if (common_tags.size() > 0) {
    return common_tags;
  }

  common_tags.add("nf.app", "api");
  common_tags.add("nf.cluster", "api-prod-tvui");
  common_tags.add("nf.asg", "api-prod-tvui-v000");
  common_tags.add("nf.ami", "ami-prod-tvui-v000");
  common_tags.add("nf.vmtype", "r3.4xlarge");
  common_tags.add("nf.zone", "us-east-1a");
  common_tags.add("nf.node", "i-12345678");

  return common_tags;
}

static TagsValuePairs load_metrics(char* buffer) {
  TagsValuePairs result;
  rapidjson::Document d;
  d.Parse(buffer);
  free(buffer);

  auto common_tags = get_common_tags();
  auto metrics = d.GetArray();
  for (auto& m : metrics) {
    Tags tags;
    auto name = m["name"].GetString();
    for (auto& t : m["tags"].GetObject()) {
      tags.add(t.name.GetString(), t.value.GetString());
    }
    auto value = m["value"].GetDouble();
    auto id = std::make_shared<Id>(name, tags);
    result.emplace_back(
        std::make_shared<IdTagsValuePair>(id, &common_tags, value));
  }

  Logger()->info("Loaded {} measurements", result.size());
  return result;
}

static void eval_subs(const Subscriptions& subs,
                      const TagsValuePairs& measurements) {
  Evaluator evaluator;
  benchmark::DoNotOptimize(generate_sub_results(evaluator, subs, measurements));
}

static void BM_subs(benchmark::State& state) {
  atlas::util::UseConsoleLogger(1);
  auto contents = read_file("subs.json");
  if (contents == nullptr) {
    exit(1);
  }
  auto subs = load_subs(contents);

  contents = read_file("api-metrics.json");
  if (contents == nullptr) {
    exit(2);
  }
  auto metrics = load_metrics(contents);

  if (subs.empty() || metrics.empty()) {
    exit(3);
  }
  for (auto _ : state) {
    eval_subs(subs, metrics);
  }
}

BENCHMARK(BM_subs);
BENCHMARK_MAIN();
