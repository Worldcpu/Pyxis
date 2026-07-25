#include <benchmark/benchmark.h>

#include <random>

#include "bravo_yen.h"
#include "graph_builder.h"
#include "px/core/astar.h"
#include "px/module/router/yen.h"

namespace {

void MakeGraph(int V, int extra, std::vector<px::RawWaypoint>& w,
               std::vector<px::RawSegment>& s) {
  std::mt19937 rng(42);
  std::uniform_real_distribution<double> lat(-2.0, 2.0);
  std::uniform_real_distribution<double> lon(0.0, 6.0);
  w.clear();
  s.clear();
  for (int i = 0; i < V; ++i)
    w.push_back({"W" + std::to_string(i), "ZZ", lat(rng), lon(rng)});
  for (int i = 0; i + 1 < V; ++i)
    s.push_back({"W" + std::to_string(i), "ZZ", "W" + std::to_string(i + 1),
                 "ZZ", "R0", px::AirwayDirection::kBoth, px::AirwayLevel::kHigh,
                 0, 999});
  std::uniform_int_distribution<int> pick(0, V - 1);
  for (int e = 0; e < extra; ++e) {
    int a = pick(rng), b = pick(rng);
    if (a != b)
      s.push_back({"W" + std::to_string(a), "ZZ", "W" + std::to_string(b), "ZZ",
                   "X" + std::to_string(e), px::AirwayDirection::kBoth,
                   px::AirwayLevel::kHigh, 0, 999});
  }
}

// 构建一次，每次迭代中搜索
class YenFixture : public benchmark::Fixture {
 public:
  void SetUp(const benchmark::State& st) override {
    std::vector<px::RawWaypoint> w;
    std::vector<px::RawSegment> s;
    MakeGraph(st.range(0), st.range(1), w, s);
    builder_ = new px::GraphBuilder(w, s);
    g_ = &builder_->graph();
    start_ = 0;
    goal_ = g_->VertexCount() - 1;
    k_ = st.range(2);
  }
  void TearDown(const benchmark::State&) override { delete builder_; }

  px::GraphBuilder* builder_;
  const px::NavGraph* g_;
  int start_, goal_, k_;
};

BENCHMARK_DEFINE_F(YenFixture, Pyxis)(benchmark::State& st) {
  for (auto _ : st) {
    auto r = px::FindKShortestPaths(*g_, start_, goal_, k_, px::YenOptions{});
    benchmark::DoNotOptimize(r);
  }
}

BENCHMARK_DEFINE_F(YenFixture, Bravo)(benchmark::State& st) {
  for (auto _ : st) {
    auto r =
        bravo::FindKShortestPaths(*g_, start_, goal_, k_, px::SearchOptions{});
    benchmark::DoNotOptimize(r);
  }
}

// 参数: {V, extra_edges, k}
// 小规模 → 真实规模（50K = 真实图规模的 1/5）
BENCHMARK_REGISTER_F(YenFixture, Pyxis)
    ->Args({500, 300, 3})
    ->Args({500, 300, 5})
    ->Args({1000, 500, 5})
    ->Args({1000, 500, 8})
    ->Args({2000, 1000, 5})
    ->Args({2000, 1000, 8})
    ->Args({5000, 2000, 5})
    ->Args({10000, 3000, 3})
    ->Args({50000, 5000, 3});
BENCHMARK_REGISTER_F(YenFixture, Bravo)
    ->Args({500, 300, 3})
    ->Args({500, 300, 5})
    ->Args({1000, 500, 5})
    ->Args({1000, 500, 8})
    ->Args({2000, 1000, 5})
    ->Args({2000, 1000, 8})
    ->Args({5000, 2000, 5})
    ->Args({10000, 3000, 3})
    ->Args({50000, 5000, 3});

}  // namespace

BENCHMARK_MAIN();
