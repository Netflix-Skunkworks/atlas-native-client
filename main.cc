
#include "atlas_client.h"
#include "util/logger.h"
#include <chrono>
#include <thread>

using namespace atlas::meter;

void init_tags(Tags* tags) {
  std::string prefix{"some.random.string.for.testing."};
  const char* value = "some.random.value.for.testing";
  for (int i = 0; i < 10; ++i) {
    tags->add((prefix + std::to_string(i)).c_str(), value);
  }
}

int main(int argc, char* argv[]) {
  atlas::util::UseConsoleLogger(1);
  const auto& logger = atlas::util::Logger();
  atlas::Client atlas_client;
  atlas_client.Start();
  auto registry = atlas_client.GetRegistry();

  Tags test_tags;
  init_tags(&test_tags);
  std::string prefix{"atlas.client.test."};
  for (int minute = 0; minute < 5; ++minute) {
    logger->info("Starting to generate metrics");
    for (int i = 0; i < 5; ++i) {
      registry->counter(prefix + std::to_string(i), test_tags)->Increment();
    }
    logger->info("Sleeping for 1s");
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  logger->info("Shutting down");

  atlas_client.Stop();
  return 0;
}
