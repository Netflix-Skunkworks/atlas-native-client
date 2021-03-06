#include <../util/logger.h>
#include <gtest/gtest.h>

int main(int argc, char** argv) {
  atlas::util::UseConsoleLogger(1);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
