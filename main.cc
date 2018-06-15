
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
  atlas::Client atlas_client;
  atlas_client.Start();
  auto registry = atlas_client.GetRegistry();

  Tags test_tags;
  init_tags(&test_tags);
  std::string prefix{"atlas.client.test."};
  auto logger = atlas::util::Logger();
  for (int second = 0; second < 60; ++second) {
    auto name = prefix + std::to_string(1);
    logger->info("Incrementing {}", name);
    registry->counter(name, kEmptyTags)->Increment();
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  logger->info("Shutting down");

  atlas_client.Stop();
  return 0;
}
