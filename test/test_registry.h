#pragma once

#include "../meter/manual_clock.h"
#include "../meter/atlas_registry.h"

class TestRegistry : public atlas::meter::AtlasRegistry {
 public:
  TestRegistry(const atlas::meter::ManualClock* manual_clock)
      : atlas::meter::AtlasRegistry(60000, manual_clock),
        manual_clock_(manual_clock) {}

  void SetWall(int64_t millis) { manual_clock_->SetWall(millis); }

  atlas::meter::Measurements measurements_for_name(const char* name) {
    auto ms = measurements();
    auto mine = atlas::meter::Measurements{};
    std::copy_if(ms.begin(), ms.end(), std::back_inserter(mine),
                 [&name](const atlas::meter::Measurement& m) {
                   return strcmp(m.id->Name(), name) == 0;
                 });
    return mine;
  }

  Meters my_meters() const {
    auto ms = meters();
    auto new_end = std::remove_if(
        ms.begin(), ms.end(),
        [](const std::shared_ptr<atlas::meter::Meter>& m) {
          return atlas::util::StartsWith(m->GetId()->NameRef(), "atlas.");
        });
    ms.resize(std::distance(ms.begin(), new_end));
    return ms;
  }

  const atlas::meter::ManualClock* manual_clock() const {
    return manual_clock_;
  }

  void expire() { expire_meters(); }

 private:
  const atlas::meter::ManualClock* manual_clock_;
};
