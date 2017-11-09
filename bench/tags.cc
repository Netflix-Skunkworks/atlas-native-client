#include "../util/intern.h"
#include <benchmark/benchmark.h>
#include <unordered_map>

namespace std {
template <>
struct less<atlas::util::StrRef> {
  bool operator()(const atlas::util::StrRef& lhs,
                  const atlas::util::StrRef& rhs) const {
    return strcmp(lhs.get(), rhs.get()) < 0;
  }
};
}  // namespace std

namespace atlas {

template <typename C>
class Tags {
  using table_t = C;
  using value_t = typename table_t::key_type;
  table_t entries_;

 public:
  void add(const char* k, const char* v) {
    entries_[util::intern_str(k)] = util::intern_str(v);
  }

  bool operator==(const Tags<C>& that) const {
    return that.entries_ == entries_;
  }

  value_t at(value_t key) const {
    auto it = entries_.find(key);
    if (it != entries_.end()) {
      return it->second;
    }
    return util::intern_str("");  // cannot throw exceptions on nodejs
  }
};

template <typename C>
static void add_tags(benchmark::State& state) {
  std::string prefix{"some.random.string.for.testing."};
  const char* value = "some.random.value.for.testing";
  for (auto _ : state) {
    Tags<C> tags;
    for (int i = 0; i < state.range(0); ++i) {
      tags.add((prefix + std::to_string(i)).c_str(), value);
    }
  }
}

template <typename C>
static Tags<C> get_tags(int keys) {
  std::string prefix{"nf.random"};
  const char* value = "some.random.value.for.testing";
  Tags<C> tags;
  for (int i = 0; i < keys; ++i) {
    tags.add((prefix + std::to_string(i)).c_str(), value);
  }
  return tags;
}

using UnorderedMap = std::unordered_map<util::StrRef, util::StrRef>;
using OrderedMap = std::map<util::StrRef, util::StrRef>;

static void BM_TagsUnorderedMap(benchmark::State& state) {
  add_tags<UnorderedMap>(state);
}
BENCHMARK(BM_TagsUnorderedMap)->RangeMultiplier(2)->Range(4, 32);

static void BM_TagsOrdered(benchmark::State& state) {
  add_tags<OrderedMap>(state);
}
BENCHMARK(BM_TagsOrdered)->RangeMultiplier(2)->Range(4, 32);

template <typename C>
static void do_tags_get(benchmark::State& state) {
  Tags<C> tags = get_tags<C>(state.range(0));

  for (auto _ : state) {
    for (int i = 0; i < state.range(1); ++i) {
      char buf[256];
      snprintf(buf, sizeof buf, "nf.random%d", i);
      auto ref = util::intern_str(buf);
      benchmark::DoNotOptimize(tags.at(ref));
    }
  }
}

static void BM_TagsUnorderedGet(benchmark::State& state) {
  do_tags_get<UnorderedMap>(state);
}

static void BM_TagsOrderedGet(benchmark::State& state) {
  do_tags_get<OrderedMap>(state);
}

BENCHMARK(BM_TagsUnorderedGet)->RangeMultiplier(2)->Ranges({{4, 32}, {8, 16}});
BENCHMARK(BM_TagsOrderedGet)->RangeMultiplier(2)->Ranges({{4, 32}, {8, 16}});

}  //  namespace atlas
BENCHMARK_MAIN();
