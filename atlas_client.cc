#include "atlas_client.h"
#include "interpreter/interpreter.h"
#include "meter/subscription_manager.h"
#include "util/config_manager.h"
#include "util/http.h"
#include "util/logger.h"
#include <iostream>

using atlas::meter::Registry;
using atlas::meter::SubscriptionManager;
using atlas::meter::WrappedClock;
using atlas::util::ConfigManager;
using atlas::util::http;
using atlas::util::Logger;

std::vector<const char*>& env_vars() {
  static std::vector<const char*> vars{"NETFLIX_CLUSTER", "EC2_OWNER_ID",
                                       "EC2_REGION", "NETFLIX_ENVIRONMENT"};
  return vars;
}

inline bool is_sane_environment() {
  auto& env = env_vars();
  return std::all_of(env.begin(), env.end(), [](const char* var) {
    return std::getenv(var) != nullptr;
  });
}

namespace atlas {

class Client::impl {
 public:
  impl()
      : started{false},
        clock{&system_clock},
        config_manager{util::kLocalFileName, util::kConfigRefreshMillis},
        subscription_manager{&clock, config_manager} {}

  ~impl() {
    if (started) {
      Stop();
    }
  }
  void Start() noexcept {
    if (started) {
      return;
    }
    http::global_init();
    const auto& logger = Logger();
    const auto& cfg = GetConfig();
    if (cfg->ShouldForceStart() || is_sane_environment()) {
      logger->info("Initializing atlas-client");
      started = true;
      config_manager.Start();
      subscription_manager.Start();
      logger->info("atlas-client initialized");
    } else {
      logger->error("Not sending metrics from a development environment");
      for (const char* v : env_vars()) {
        const auto env_value = std::getenv(v);
        const auto value = env_value == nullptr ? "(null)" : env_value;
        logger->info("{}={}", v, value);
      }
    }
  }

  void Stop() noexcept {
    auto logger = Logger();
    if (started) {
      logger->info("Stopping atlas-client");
      subscription_manager.Stop();
      config_manager.Stop();
      started = false;
      http::global_shutdown();
    } else {
      logger->info("Ignoring stop request since we were never started");
    }
  }

  void Push(const atlas::meter::Measurements& measurements) noexcept {
    using atlas::interpreter::TagsValuePair;
    using atlas::interpreter::TagsValuePairs;
    using atlas::meter::Measurement;

    TagsValuePairs tagsValuePairs;
    std::transform(
        measurements.begin(), measurements.end(),
        std::back_inserter(tagsValuePairs), [](const Measurement& measurement) {
          return TagsValuePair::from(measurement, &atlas::meter::kEmptyTags);
        });

    subscription_manager.PushMeasurements(clock.WallTime(), tagsValuePairs);
  }

  std::shared_ptr<atlas::util::Config> GetConfig() noexcept {
    return config_manager.GetConfig();
  }

  void AddCommonTag(const char* key, const char* value) noexcept {
    config_manager.AddCommonTag(key, value);
  }

  std::shared_ptr<meter::Registry> GetRegistry() {
    return subscription_manager.GetRegistry();
  }

 private:
  bool started;
  atlas::meter::SystemClock system_clock;
  WrappedClock clock;
  ConfigManager config_manager;
  SubscriptionManager subscription_manager;
};

Client::Client() noexcept : impl_{std::make_unique<impl>()} {}

void Client::SetLoggingDirs(const std::vector<std::string>& dirs) noexcept {
  atlas::util::SetLoggingDirs(dirs);
}

void Client::UseConsoleLogger(int level) noexcept {
  atlas::util::UseConsoleLogger(level);
}

std::shared_ptr<Registry> Client::GetRegistry() const noexcept {
  return impl_->GetRegistry();
}

void Client::Start() noexcept { impl_->Start(); }

void Client::Stop() noexcept { impl_->Stop(); }

std::shared_ptr<util::Config> Client::GetConfig() noexcept {
  return impl_->GetConfig();
}

void Client::Push(const meter::Measurements& measurements) noexcept {
  impl_->Push(measurements);
}

void Client::AddCommonTag(const char* key, const char* value) noexcept {
  impl_->AddCommonTag(key, value);
}

Client::~Client() noexcept {}

}  // namespace atlas
