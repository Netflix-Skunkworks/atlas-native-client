#include "atlas_client.h"
#include "interpreter/interpreter.h"
#include "meter/subscription_manager.h"
#include "util/config_manager.h"
#include "util/http.h"
#include "util/logger.h"
#include <iostream>

using atlas::interpreter::ClientVocabulary;
using atlas::interpreter::Interpreter;
using atlas::meter::SubscriptionManager;
using atlas::meter::SubscriptionRegistry;
using atlas::meter::SystemClockWithOffset;
using atlas::util::ConfigManager;
using atlas::util::Logger;
using atlas::util::http;

static SystemClockWithOffset clockWithOffset;
SubscriptionRegistry atlas_registry{
    std::make_unique<Interpreter>(std::make_unique<ClientVocabulary>()),
    &clockWithOffset};

const std::vector<const char*> env_vars{"NETFLIX_CLUSTER", "EC2_OWNER_ID",
                                        "EC2_REGION", "NETFLIX_ENVIRONMENT"};

inline bool is_sane_environment() {
  return std::all_of(env_vars.begin(), env_vars.end(), [](const char* var) {
    return std::getenv(var) != nullptr;
  });
}

class AtlasClient {
 public:
  AtlasClient() noexcept
      : started{false},
        subscription_manager{
            new SubscriptionManager(config_manager, atlas_registry)} {}

  void Start() {
    if (started) {
      return;
    }
    http::global_init();
    auto logger = Logger();
    auto cfg = GetConfig();
    if (cfg->ShouldForceStart() || is_sane_environment()) {
      logger->info("Initializing atlas-client");
      started = true;
      config_manager.Start();
      subscription_manager->Start();
    } else {
      logger->error("Not sending metrics from a development environment.");
      for (const char* v : env_vars) {
        const auto env_value = std::getenv(v);
        const auto value = env_value == nullptr ? "(null)" : env_value;
        logger->info("{}={}", v, value);
      }
    }
  }

  void Stop() {
    if (started) {
      Logger()->info("Stopping atlas-client");
      subscription_manager->Stop(&clockWithOffset);
      config_manager.Stop();
      started = false;
      http::global_shutdown();
    }
  }

  std::shared_ptr<atlas::util::Config> GetConfig() {
    return config_manager.GetConfig();
  }

  void Push(const atlas::meter::Measurements& measurements) {
    using atlas::interpreter::TagsValuePair;
    using atlas::interpreter::TagsValuePairs;
    using atlas::meter::Measurement;

    TagsValuePairs tagsValuePairs;
    std::transform(
        measurements.begin(), measurements.end(),
        std::back_inserter(tagsValuePairs), [](const Measurement& measurement) {
          return TagsValuePair::from(measurement, &atlas::meter::kEmptyTags);
        });

    subscription_manager->PushMeasurements(atlas_registry.clock().WallTime(),
                                           tagsValuePairs);
  }

  void AddCommonTag(const char* key, const char* value) {
    config_manager.AddCommonTag(key, value);
  }

  void SetNotifyAlertServer(bool notify) {
    config_manager.SetNotifyAlertServer(notify);
  }

 private:
  bool started;
  ConfigManager config_manager;
  std::unique_ptr<SubscriptionManager> subscription_manager;
};

static auto atlas_client = std::unique_ptr<AtlasClient>(nullptr);

namespace atlas {
util::Config GetConfig() {
  if (atlas_client) {
    return *atlas_client->GetConfig();
  }
  return util::Config();
}

void PushMeasurements(const atlas::meter::Measurements& measurements) {
  if (atlas_client) {
    atlas_client->Push(measurements);
  }
}

void SetLoggingDirs(const std::vector<std::string>& dirs) {
  atlas::util::SetLoggingDirs(dirs);
}

void UseConsoleLogger(int level) { atlas::util::UseConsoleLogger(level); }

void SetNotifyAlertServer(bool notify) {
  if (atlas_client) {
    atlas_client->SetNotifyAlertServer(notify);
  }
}

void Init() {
  if (!atlas_client) {
    atlas_client = std::make_unique<AtlasClient>();
  }
  atlas_client->Start();
}

void Shutdown() {
  if (atlas_client) {
    atlas_client->Stop();
  }
}

void AddCommonTag(const char* key, const char* value) {
  if (atlas_client) {
    atlas_client->AddCommonTag(key, value);
  }
}

}  // namespace atlas

void InitAtlas() { atlas::Init(); }

void ShutdownAtlas() { atlas::Shutdown(); }

void AtlasAddCommonTag(const char* key, const char* value) {
  atlas::AddCommonTag(key, value);
}
