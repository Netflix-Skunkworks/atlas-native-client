#pragma once

#include "../meter/manual_clock.h"
#include "../meter/atlas_registry.h"

class TestRegistry : public atlas::meter::AtlasRegistry {
 public:
  TestRegistry() : atlas::meter::AtlasRegistry(60000, &manual_clock) {}

  void SetWall(int64_t millis) { manual_clock.SetWall(millis); }

  atlas::meter::Measurements measurements_for_name(const char* name) {
    auto ms = measurements();
    auto mine = atlas::meter::Measurements{};
    std::copy_if(ms.begin(), ms.end(), std::back_inserter(mine),
                 [&name](const atlas::meter::Measurement& m) {
                   return strcmp(m.id->Name(), name) == 0;
                 });
    return mine;
  }

 private:
  atlas::meter::ManualClock manual_clock{};
};
