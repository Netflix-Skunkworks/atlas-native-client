#include <gtest/gtest.h>
#include "../util/collections.h"
#include <string>

TEST(Vector, Concat) {
  std::vector<int> v1{1, 2, 3};
  std::vector<int> v2{4, 5, 6};

  auto result = vector_concat(std::move(v1), std::move(v2));
  std::vector<int> expected{1, 2, 3, 4, 5, 6};
  EXPECT_EQ(expected, result);
}

TEST(Vector, Append_to) {
  std::vector<int> v1{1, 2, 3};
  std::vector<int> v2{4, 5, 6};
  std::vector<int> v3{7, 8, 9};

  std::vector<int> result;
  append_to_vector(&result, std::move(v1));
  append_to_vector(&result, std::move(v2));
  append_to_vector(&result, std::move(v3));
  std::vector<int> expected{1, 2, 3, 4, 5, 6, 7, 8, 9};
  EXPECT_EQ(expected, result);
}

TEST(Vector, Cross) {
  std::vector<std::string> v1{"a", "b"};
  std::vector<std::string> v2{"c", "d"};

  auto res = vector_cross_product(
      v1, v2, [](const std::string& a, const std::string& b) { return a + b; });
  std::vector<std::string> expected{"ac", "ad", "bc", "bd"};
  EXPECT_EQ(res, expected);
};

TEST(Set, Append_to) {
  std::unordered_set<int> v1{1, 2, 3};
  std::unordered_set<int> v2{4, 5, 6};
  std::unordered_set<int> v3{7, 8, 9};

  std::unordered_set<int> result;
  append_to_set(&result, std::move(v1));
  append_to_set(&result, std::move(v2));
  append_to_set(&result, std::move(v3));
  std::unordered_set<int> expected{1, 2, 3, 4, 5, 6, 7, 8, 9};
  EXPECT_EQ(expected, result);
}
