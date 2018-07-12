
#include "atlas_client.h"
#include "util/logger.h"
#include <chrono>
#include <thread>

using namespace atlas::meter;

int main(int argc, char* argv[]) {
  atlas::util::UseConsoleLogger(1);
  atlas::Client atlas_client;
  atlas_client.Start();
  auto registry = atlas_client.GetRegistry();

  std::string name{"test.counter"};
  auto logger = atlas::util::Logger();
  for (int second = 0; second < 60; ++second) {
    registry->counter(name, kEmptyTags)->Increment();
    logger->info("Incrementing test.counter: {} ({})", second + 1,
                 (second + 1) / 60.0);
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  logger->info("Shutting down");

  atlas_client.Stop();
  return 0;
}
