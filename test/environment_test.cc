#include "../util/environment.h"
#include <gtest/gtest.h>

using namespace atlas::util;

TEST(Environment, id) { EXPECT_FALSE(env::instance_id().empty()); }
