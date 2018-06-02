#include "../util/intern.h"
#include "../meter/id.h"
#include "../util/small_tag_map.h"
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


class VectorTags {
  using value_t = util::StrRef;
  using entry_t = std::pair<value_t, value_t>;
 public:
  void add(const char* k, const char* v) {
    entries_.emplace_back(util::intern_str(k), util::intern_str(v));
  }

  bool operator==(const VectorTags& that) const {
    return that.entries_ == entries_;
  }

  value_t at(value_t key) const {
    auto it = std::find_if(entries_.rbegin(), entries_.rend(),
    [key](const entry_t& entry) { return entry.first == key; });
    if (it != entries_.rend()) {
      return it->second;
    }
    return util::intern_str("");  // cannot throw exceptions on nodejs
  }
 private:
  std::vector<entry_t> entries_;
};

template <typename T>
static void add_tags(benchmark::State& state) {
  std::string prefix{"some.random.string.for.testing."};
  const char* value = "some.random.value.for.testing";
  for (auto _ : state) {
    T tags;
    for (int i = 0; i < state.range(0); ++i) {
      tags.add((prefix + std::to_string(i)).c_str(), value);
    }
  }
}

template <typename T>
static T get_tags(int keys) {
  std::string prefix{"nf.random"};
  const char* value = "some.random.value.for.testing";
  T tags;
  for (int i = 0; i < keys; ++i) {
    tags.add((prefix + std::to_string(i)).c_str(), value);
  }
  return tags;
}

using UnorderedMap = std::unordered_map<util::StrRef, util::StrRef>;
using OrderedMap = std::map<util::StrRef, util::StrRef>;
using util::SmallTagMap;

static void BM_TagsUnorderedMap(benchmark::State& state) {
  add_tags<Tags<UnorderedMap>>(state);
}
BENCHMARK(BM_TagsUnorderedMap)->RangeMultiplier(2)->Range(2, 32);

static void BM_TagsOrdered(benchmark::State& state) {
  add_tags<Tags<OrderedMap>>(state);
}
BENCHMARK(BM_TagsOrdered)->RangeMultiplier(2)->Range(2, 32);

static void BM_VectorTags(benchmark::State& state) {
  add_tags<VectorTags>(state);
}
BENCHMARK(BM_VectorTags)->RangeMultiplier(2)->Range(2, 32);

static void BM_SmallTags(benchmark::State& state) {
  add_tags<SmallTagMap>(state);
}
BENCHMARK(BM_SmallTags)->RangeMultiplier(2)->Range(2, 32);

template <typename T>
static void do_tags_get(benchmark::State& state) {
  auto non_existing = util::intern_str("does.not.exist");
  T tags = get_tags<T>(state.range(0));

  for (auto _ : state) {
    int N = state.range(1);
    for (int i = 0; i < N; ++i) {
      int random_key = rand() % N;
      char buf[32];
      snprintf(buf, sizeof buf, "nf.random%d", random_key);
      auto ref = util::intern_str(buf);
      benchmark::DoNotOptimize(tags.at(ref));
      benchmark::DoNotOptimize(tags.at(non_existing));
    }
  }
}

static void BM_TagsUnorderedGet(benchmark::State& state) {
  do_tags_get<Tags<UnorderedMap>>(state);
}

static void BM_TagsOrderedGet(benchmark::State& state) {
  do_tags_get<Tags<OrderedMap>>(state);
}

static void BM_VectorTagsGet(benchmark::State& state) {
  do_tags_get<VectorTags>(state);
}

static void BM_SmallTagsGet(benchmark::State& state) {
  do_tags_get<SmallTagMap>(state);
}

BENCHMARK(BM_TagsUnorderedGet)->RangeMultiplier(2)->Ranges({{8, 24}, {8, 16}});
BENCHMARK(BM_TagsOrderedGet)->RangeMultiplier(2)->Ranges({{8, 24}, {8, 16}});
BENCHMARK(BM_VectorTagsGet)->RangeMultiplier(2)->Ranges({{8, 24}, {8, 16}});
BENCHMARK(BM_SmallTagsGet)->RangeMultiplier(2)->Ranges({{8, 24}, {8, 16}});

}  //  namespace atlas
BENCHMARK_MAIN();
