// This is just an example of something using the meter api

#include "atlas_client.h"
#include "util/logger.h"
#include <chrono>
#include <thread>

using namespace atlas::meter;

std::shared_ptr<Counter> counter(std::string&& name, const Tags& tags) {
  return atlas_registry.counter(atlas_registry.CreateId(name, tags));
}

void init_tags(Tags* tags) {
  std::string prefix{"some.random.string.for.testing."};
  const char* value = "some.random.value.for.testing";
  for (int i = 0; i < 10; ++i) {
    tags->add((prefix + std::to_string(i)).c_str(), value);
  }
}

int main(int argc, char* argv[]) {
  atlas::util::UseConsoleLogger();
  auto logger = atlas::util::Logger();
  InitAtlas();

  Tags test_tags;
  init_tags(&test_tags);
  std::string prefix{"atlas.client.test."};
  for (int minute = 0; minute < 5; ++minute) {
    logger->info("Starting to generate 50k metrics");
    for (int i = 0; i < 50000; ++i) {
      counter(prefix + std::to_string(i), test_tags)->Increment();
    }
    logger->info("Sleeping for 40s");
    std::this_thread::sleep_for(std::chrono::seconds(40));
  }

  ShutdownAtlas();
  return 0;
}
