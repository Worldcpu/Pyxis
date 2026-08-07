// SPDX-License-Identifier: MIT
// px_navdata 备降过滤 + 机场索引实现（决策 12/48）。距离计算复用
// bf::Coordinate（球面 haversine，NM）；bfdb 读取经 bf 公开 API
// （UnifiedCache）；bf 类型仅在 .cc 内出现，公开头不暴露。
#include "px/module/navdata/airport_index.h"

#include <algorithm>
#include <cctype>
#include <unordered_set>
#include <utility>

#include "bf_adapter.h"
#include "core/domain/coordinate.h"
#include "io/cache/unified_cache.h"

namespace px {

double DistanceNm(const GeoCoord& a, const GeoCoord& b) noexcept {
  return bf::Coordinate{a.latitude, a.longitude}.DistanceTo(
      bf::Coordinate{b.latitude, b.longitude});
}

std::vector<AlternateCandidate> FilterAlternates(
    std::span<const AirportEntry> airports, const GeoCoord& arrival,
    const AlternatesParams& params) {
  const bf::Coordinate arr{arrival.latitude, arrival.longitude};
  std::vector<AlternateCandidate> out;
  out.reserve(airports.size());
  // 排除列表构建为集合（O(A) 预构建，循环内 O(1)——避免每次请求 O(N·A)）。
  const std::unordered_set<std::string> avoid(params.avoid_icaos.begin(),
                                              params.avoid_icaos.end());
  for (const AirportEntry& e : airports) {
    // 仅 4 字 ICAO（决策 12 修订：排除 FAA LID 等短码机场）。
    if (e.icao.size() != 4) {
      continue;
    }
    // 到达场自身（距离 0 的假候选）与排除列表。
    if (e.icao == params.exclude_icao || avoid.count(e.icao) != 0) {
      continue;
    }
    // 距离过滤。
    const double distance_nm = DistanceNm(e.coord, arrival);
    if (distance_nm > params.max_distance_nm) {
      continue;
    }
    out.push_back(AlternateCandidate{e.icao, distance_nm, "DCT"});
  }
  // 距离升序 + 截断 limit 条（决策 12）。
  std::sort(out.begin(), out.end(),
            [](const AlternateCandidate& a, const AlternateCandidate& b) {
              return a.distance_nm < b.distance_nm;
            });
  if (out.size() > params.limit) {
    out.resize(params.limit);
  }
  return out;
}

Result<AirportIndex> AirportIndex::Open(const std::string& bfdb_path) {
  auto opened = bf::UnifiedCache::Open(bfdb_path);
  if (!opened.has_value()) {
    // 经 bf_adapter 映射：缺失 → kDataMissing，坏 magic/截断 → kCacheCorrupt。
    return Err(FromBfError(opened.error()));
  }
  const bf::GraphSnapshot& graph = opened.value().graph;
  // 防御校验（损坏但 magic 通过/未来格式漂移的 bfdb）：graph 各平行
  // 数组必须同长且机场顶点区有效——否则索引越界/下溢 reserve 会终止
  // 进程（-fno-exceptions）。返回 kCacheCorrupt 保持文档承诺的错误路径。
  const int airport_count =
      static_cast<int>(graph.coords.size()) - graph.first_airport_vertex;
  if (graph.first_airport_vertex < 0 ||
      static_cast<size_t>(graph.first_airport_vertex) > graph.coords.size() ||
      graph.coords.size() != graph.idents.size()) {
    return Err(Error(ErrorCode::kCacheCorrupt, "bfdb graph 段结构不一致"));
  }
  std::vector<AirportEntry> airports;
  airports.reserve(static_cast<size_t>(airport_count));
  for (size_t i = static_cast<size_t>(graph.first_airport_vertex);
       i < graph.coords.size(); ++i) {
    AirportEntry e;
    e.icao = std::string(graph.idents[i].IdentView());
    e.coord = {graph.coords[i].latitude, graph.coords[i].longitude};
    airports.push_back(std::move(e));
  }
  if (airports.empty()) {
    return Err(Error(ErrorCode::kInternalError, "bfdb graph 无机场顶点"));
  }
  AirportIndex index;
  index.airports_ = std::move(airports);
  return Ok(std::move(index));
}

const AirportEntry* AirportIndex::Find(const std::string& icao) const noexcept {
  // 大小写不敏感（对齐 bf ToUpper 归一化——审查修复：routes 大写匹配
  // 而 alternates 精确匹配会对同一小写字符串返回 404）。
  for (const auto& entry : airports_) {
    if (entry.icao.size() != icao.size()) continue;
    bool equal = true;
    for (size_t i = 0; i < icao.size(); ++i) {
      if (std::toupper(static_cast<unsigned char>(entry.icao[i])) !=
          std::toupper(static_cast<unsigned char>(icao[i]))) {
        equal = false;
        break;
      }
    }
    if (equal) return &entry;
  }
  return nullptr;
}

}  // namespace px
