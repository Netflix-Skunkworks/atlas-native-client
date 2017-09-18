#include "../atlas_client.h"  // for atlas_registry
#include "../interpreter/group_by.h"
#include "../interpreter/interpreter.h"
#include "../util/logger.h"
#include "subscription_counter.h"
#include "subscription_distribution_summary.h"
#include "subscription_gauge.h"
#include "subscription_long_task_timer.h"
#include "subscription_timer.h"

#include <numeric>
#include <sstream>

using atlas::util::Logger;

namespace atlas {
namespace meter {

SystemClock system_clock;

// we mostly use this class to avoid adding an external dependency to
// the interpreter from our public headers
class SubscriptionRegistry::impl {
 public:
  explicit impl(std::unique_ptr<interpreter::Interpreter> interpreter) noexcept
      : interpreter_(std::move(interpreter)) {}

  interpreter::Interpreter* GetInterpreter() const noexcept {
    return interpreter_.get();
  }

  std::shared_ptr<Meter> GetMeter(IdPtr id) noexcept {
    std::lock_guard<std::mutex> lock(meters_mutex);
    auto maybe_meter = meters_.find(id);
    if (maybe_meter != meters_.end()) {
      return maybe_meter->second;
    }
    return std::shared_ptr<Meter>(nullptr);
  }

  // only insert if it doesn't exist, otherwise return the existing meter
  std::shared_ptr<Meter> InsertIfNeeded(std::shared_ptr<Meter> meter) noexcept {
    std::lock_guard<std::mutex> lock(meters_mutex);
    auto insert_result = meters_.insert(std::make_pair(meter->GetId(), meter));
    auto ret = insert_result.first->second;
    return ret;
  }

  void UpdatePollersForMeters() const noexcept {
    std::lock_guard<std::mutex> lock(meters_mutex);
    for (auto& m : meters_) {
      m.second->UpdatePollers();
    }
  }

  Meters GetMeters() const noexcept {
    static auto meters_size = atlas_registry.gauge("atlas.client.meters");
    std::lock_guard<std::mutex> lock(meters_mutex);

    Meters res;
    res.reserve(meters_.size());
    meters_size->Update(meters_.size());

    for (auto m : meters_) {
      res.push_back(m.second);
    }
    return res;
  }

 private:
  std::unique_ptr<interpreter::Interpreter> interpreter_;
  // TODO(dmuino) use a concurrent map
  mutable std::mutex meters_mutex;
  std::unordered_map<IdPtr, std::shared_ptr<Meter>> meters_;
};

const Clock& SubscriptionRegistry::clock() const noexcept { return *clock_; }

Pollers& SubscriptionRegistry::pollers() noexcept { return poller_freq_; }

IdPtr SubscriptionRegistry::CreateId(std::string name, Tags tags) {
  return std::make_shared<Id>(name, tags);
}

std::shared_ptr<Meter> SubscriptionRegistry::GetMeter(IdPtr id) noexcept {
  return impl_->GetMeter(id);
}

// only insert if it doesn't exist, otherwise return the existing meter
std::shared_ptr<Meter> SubscriptionRegistry::InsertIfNeeded(
    std::shared_ptr<Meter> meter) noexcept {
  return impl_->InsertIfNeeded(meter);
}

std::shared_ptr<Counter> SubscriptionRegistry::counter(IdPtr id) noexcept {
  return CreateAndRegisterAsNeeded<SubscriptionCounter>(id);
}

std::shared_ptr<Timer> SubscriptionRegistry::timer(IdPtr id) noexcept {
  return CreateAndRegisterAsNeeded<SubscriptionTimer>(id);
}

std::shared_ptr<Gauge<double>> SubscriptionRegistry::gauge(IdPtr id) noexcept {
  return CreateAndRegisterAsNeededG<SubscriptionGauge>(id);
}

std::shared_ptr<LongTaskTimer> SubscriptionRegistry::long_task_timer(
    IdPtr id) noexcept {
  return CreateAndRegisterAsNeededG<SubscriptionLongTaskTimer>(id);
}

std::shared_ptr<DistributionSummary> SubscriptionRegistry::distribution_summary(
    IdPtr id) noexcept {
  return CreateAndRegisterAsNeeded<SubscriptionDistributionSummary>(id);
}

void SubscriptionRegistry::update_subscriptions(const Subscriptions* new_subs) {
  static auto num_pollers = atlas_registry.gauge("atlas.client.numPollers");
  auto pollers_updated = false;
  std::lock_guard<std::mutex> guard(subscriptions_mutex);

  subscriptions_ = new_subs;
  for (auto& s : *subscriptions_) {
    auto freq = s.frequency;
    if (!AlreadySeen(freq)) {
      poller_freq_.push_back(freq);
      pollers_updated = true;
    }
  }

  num_pollers->Update(poller_freq_.size());
  if (pollers_updated) {
    impl_->UpdatePollersForMeters();
  }
}

bool SubscriptionRegistry::AlreadySeen(int s) noexcept {
  return std::find(poller_freq_.begin(), poller_freq_.end(), s) !=
         poller_freq_.end();
}

Registry::Meters SubscriptionRegistry::meters() const noexcept {
  return impl_->GetMeters();
}

void SubscriptionRegistry::RegisterMonitor(
    std::shared_ptr<Meter> meter) noexcept {
  InsertIfNeeded(meter);
}

// FIXME: test
std::shared_ptr<interpreter::Query> QueryForSubs(
    const interpreter::Interpreter& inter, const Subscriptions& subscriptions) {
  using interpreter::Query;
  using interpreter::query::false_q;
  using interpreter::query::or_q;
  return std::accumulate(
      subscriptions.begin(), subscriptions.end(),
      std::shared_ptr<interpreter::Query>(false_q()),
      [&inter](std::shared_ptr<Query>& q, const Subscription& s) {
        return or_q(q, inter.GetQuery(s.expression));
      });
}

static void LogRules(const std::vector<std::string>& rules,
                     size_t num_measurements) {
  auto logger = Logger();
  if (logger->should_log(spdlog::level::debug)) {
    std::ostringstream os;
    os << "Applying [";
    bool first = true;
    for (const auto& rule : rules) {
      if (!first) {
        os << ", ";
      } else {
        first = false;
      }
      os << rule;
    }
    os << "] to " << num_measurements << " measurements";
    logger->debug(os.str());
  }
}

static void LogMeasurementsForRule(const std::string& rule,
                                   const interpreter::TagsValuePairs& pairs) {
  auto logger = Logger();
  if (logger->should_log(spdlog::level::debug)) {
    logger->debug("Rule: {}", rule);
    for (const auto& pair : pairs) {
      logger->debug("\t{}", pair);
    }
  }
}

interpreter::TagsValuePairs SubscriptionRegistry::GetMainMeasurements(
    const util::Config& config) const {
  static auto main_measurements_size =
      atlas_registry.gauge("atlas.client.mainMeasurements");
  static auto raw_measurements_size =
      atlas_registry.gauge("atlas.client.rawMainMeasurements");

  using interpreter::Query;
  using interpreter::TagsValuePairs;
  using interpreter::TagsValuePair;

  auto logger = Logger();
  const auto& all = GetMeasurements(util::kMainFrequencyMillis);
  TagsValuePairs result;

  raw_measurements_size->Update(all.size());
  if (all.empty()) {
    logger->info("No metrics registered.");
    return result;
  }

  // apply the rollup config

  const auto& rules = config.PublishConfig();
  const auto& common_tags = config.CommonTags();
  if (rules.empty()) {
    logger->info("No publish configuration. Assuming :all for {} measurements.",
                 all.size());
    std::transform(all.begin(), all.end(), std::back_inserter(result),
                   [&common_tags](const Measurement& m) {
                     return TagsValuePair::from(m, common_tags);
                   });
    return result;
  }
  LogRules(rules, all.size());

  // queries for each rule
  std::vector<std::shared_ptr<Query>> queries;
  std::transform(rules.begin(), rules.end(), std::back_inserter(queries),
                 [this](const std::string& rule) {
                   return impl_->GetInterpreter()->GetQuery(rule);
                 });

  // metrics that match each rule
  auto measurements_for_rule =
      std::unique_ptr<TagsValuePairs[]>(new TagsValuePairs[rules.size()]);

  for (const auto& m : all) {
    auto tagsValue = TagsValuePair::from(m, common_tags);
    for (size_t i = 0; i < rules.size(); ++i) {
      const auto& query = queries[i];

      if (query->Matches(tagsValue.tags)) {
        measurements_for_rule[i].push_back(tagsValue);
        break;
      }
    }
    // drop if we don't match
  }

  // measurements for each rule have been collected
  // now apply the rules
  auto i = 0;
  for (const auto& rule : rules) {
    // create a dummy subscription to re-use the evaluation engine in the
    // subscription_registry
    LogMeasurementsForRule(rule, measurements_for_rule[i]);
    auto rule_result = evaluate(rule, measurements_for_rule[i]);
    ++i;
    // add all resulting measurements to our overall result
    std::move(rule_result.begin(), rule_result.end(),
              std::back_inserter(result));
  }

  main_measurements_size->Update(result.size());
  return result;
}

// TODO(dmuino): test
SubscriptionResults SubscriptionRegistry::GetLwcMetricsForInterval(
    const util::Config& config, int64_t frequency) const {
  using interpreter::TagsValuePair;
  using interpreter::TagsValuePairs;
  static auto subsId =
      atlas_registry.CreateId("atlas.client.lwcSubs", kEmptyTags);
  static auto measurementsId =
      atlas_registry.CreateId("atlas.client.lwcMeasurements", kEmptyTags);

  SubscriptionResults result;
  // get all the subscriptions for a given interval (ignoring main)
  auto subs = SubsForInterval(frequency);
  auto frequency_str = std::to_string(frequency).c_str();
  atlas_registry.gauge(subsId->WithTag(Tag::of("freq", frequency_str)))
      ->Update(subs.size());

  // get all the measurements that will be used
  auto measurements = GetMeasurements(frequency);

  // gather all metrics generated by our subscriptions
  const auto& common_tags = config.CommonTags();
  for (auto& s : subs) {
    TagsValuePairs tagsValuePairs;
    std::transform(measurements.begin(), measurements.end(),
                   std::back_inserter(tagsValuePairs),
                   [&common_tags](const Measurement& m) {
                     return TagsValuePair::from(m, common_tags);
                   });
    auto pairs = evaluate(s.expression, tagsValuePairs);
    std::transform(pairs.begin(), pairs.end(), std::back_inserter(result),
                   [&s](const TagsValuePair& pair) {
                     return SubscriptionMetric{s.id, pair.tags, pair.value};
                   });
  }
  atlas_registry.gauge(measurementsId->WithTag(Tag::of("freq", frequency_str)))
      ->Update(result.size());
  return result;
}

Measurements SubscriptionRegistry::GetMeasurements(int64_t frequency) const {
  Measurements res;
  // quickly create a copy of the meters to avoid locking while we get the
  // measurements
  const auto all_meters = meters();

  // attempt to guess how big the resulting measurements will be
  res.reserve(all_meters.size() *
              2);  // timers / distribution summaries = 4x, counters = 1x

  auto pos = std::find(poller_freq_.begin(), poller_freq_.end(), frequency);
  if (pos == poller_freq_.end()) {
    Logger()->error("Unable to find poller frequency: {}", frequency);
  } else {
    auto poller_idx =
        static_cast<size_t>(std::distance(poller_freq_.begin(), pos));

    // now get a measurement from each meter that is involved in the given query
    for (const auto& m : all_meters) {
      if (!m->HasExpired()) {
        if (m->IsUpdateable()) {
          std::static_pointer_cast<UpdateableMeter>(m)->Update();
        }
        const auto measurements = m->MeasuresForPoller(poller_idx);
        if (!measurements.empty()) {
          std::move(measurements.begin(), measurements.end(),
                    std::back_inserter(res));
        }
      }
    }
  }
  return res;
}

Subscriptions SubscriptionRegistry::SubsForInterval(int64_t frequency) const
    noexcept {
  std::lock_guard<std::mutex> guard(subscriptions_mutex);
  Subscriptions res;
  std::copy_if(
      std::begin(*subscriptions_), std::end(*subscriptions_),
      std::back_inserter(res), [frequency](const Subscription& subscription) {
        return subscription.frequency == frequency && !subscription.id.empty();
      });

  return res;
}

SubscriptionRegistry::SubscriptionRegistry(
    std::unique_ptr<interpreter::Interpreter> interpreter,
    const Clock* clock) noexcept
    : impl_(std::make_unique<impl>(std::move(interpreter))),
      clock_{clock},
      poller_freq_{util::kMainFrequencyMillis} {}

interpreter::TagsValuePairs SubscriptionRegistry::evaluate(
    const std::string& expression,
    const interpreter::TagsValuePairs& tagsValuePairs) const {
  interpreter::TagsValuePairs results;
  if (tagsValuePairs.empty()) {
    return results;
  }
  auto stack = std::make_unique<interpreter::Context::Stack>();
  auto context = std::make_unique<interpreter::Context>(std::move(stack));
  impl_->GetInterpreter()->Execute(context.get(), expression);

  // for each expression on the stack
  while (context->StackSize() > 0) {
    auto expr = context->PopExpression();
    auto by = interpreter::expression::GetMultipleResults(std::move(expr));
    if (by) {
      auto expression_result = by->Apply(tagsValuePairs);
      std::move(expression_result.begin(), expression_result.end(),
                std::back_inserter(results));
    }
  }

  return results;
}

SubscriptionRegistry::~SubscriptionRegistry() = default;

}  // namespace meter
}  // namespace atlas
