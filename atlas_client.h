#pragma once

#include "meter/subscription_registry.h"
#include <vector>

extern "C" void InitAtlas();
extern "C" void ShutdownAtlas();
extern "C" void AtlasAddCommonTag(const char* key, const char* value);

extern atlas::meter::SubscriptionRegistry atlas_registry;

atlas::util::Config GetConfig();
void PushMeasurements(const atlas::meter::Measurements& measurements);
void SetLoggingDirs(const std::vector<std::string>& dirs);
