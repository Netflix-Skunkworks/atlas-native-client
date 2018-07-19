#pragma once
#include <vector>

template <typename T>
void append_to_vector(std::vector<T>* dest, std::vector<T>&& to_append) {
  dest->reserve(dest->size() + to_append.size());
  for (auto&& item : to_append) {
    dest->emplace_back(std::move(item));
  }
}

template <typename T>
std::vector<T> vector_concat(std::vector<T>&& v1, std::vector<T>&& v2) {
  std::vector<T> result{v1};
  append_to_vector(&result, std::move(v2));
  return result;
}

template <typename T, class Fact>
std::vector<T> vector_cross_product(const std::vector<T>& v1,
                                    const std::vector<T>& v2, Fact factory) {
  std::vector<T> result;
  result.reserve(v1.size() * v2.size());
  for (auto& i1 : v1) {
    for (auto& i2 : v2) {
      result.emplace_back(factory(i1, i2));
    }
  }
  return result;
}