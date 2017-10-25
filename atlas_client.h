#pragma once

#include "meter/subscription_registry.h"
#include <vector>

extern "C" void InitAtlas();
extern "C" void ShutdownAtlas();
extern "C" void AtlasAddCommonTag(const char* key, const char* value);

extern atlas::meter::SubscriptionRegistry atlas_registry;

namespace atlas {
util::Config GetConfig();
void PushMeasurements(const atlas::meter::Measurements& measurements);
void SetLoggingDirs(const std::vector<std::string>& dirs);
void UseConsoleLogger(int level);
void SetNotifyAlertServer(bool notify);
}

/* Deprecated functions. Please use functions in namespace atlas instead. */
inline atlas::util::Config GetConfig() { return atlas::GetConfig(); }

inline void PushMeasurements(const atlas::meter::Measurements& measurements) {
  atlas::PushMeasurements(measurements);
}

inline void SetLoggingDirs(const std::vector<std::string>& dirs) {
  atlas::SetLoggingDirs(dirs);
}

inline void UseConsoleLogger(int level) { atlas::UseConsoleLogger(level); }
/* End deprecated functions */
