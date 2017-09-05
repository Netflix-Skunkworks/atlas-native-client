#pragma once

#include "../interpreter/interpreter.h"
#include "../meter/manual_clock.h"
#include "../meter/subscription_registry.h"

class TestRegistry : public atlas::meter::SubscriptionRegistry {
 public:
  TestRegistry()
      : atlas::meter::SubscriptionRegistry(
            std::make_unique<atlas::interpreter::Interpreter>(
                std::make_unique<atlas::interpreter::ClientVocabulary>()),
            &manual_clock) {}
  atlas::meter::Measurements AllMeasurements() const {
    return GetMeasurements(atlas::util::kMainFrequencyMillis);
  }

  void SetWall(int64_t millis) { manual_clock.SetWall(millis); }

 private:
  atlas::meter::ManualClock manual_clock;
};
