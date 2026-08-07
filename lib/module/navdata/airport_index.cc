// SPDX-License-Identifier: MIT
// px_navdata 备降过滤 + 机场索引实现（决策 12/48）。距离计算复用
// bf::Coordinate（球面 haversine，NM）；bfdb 读取经 bf 公开 API
// （UnifiedCache）；bf 类型仅在 .cc 内出现，公开头不暴露。
#include "px/module/navdata/airport_index.h"

#include <algorithm>
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
  for (const AirportEntry& e : airports) {
    // 仅 4 字 ICAO（决策 12 修订：排除 FAA LID 等短码机场）。
    if (e.icao.size() != 4) {
      continue;
    }
    // 排除列表。
    if (std::find(params.avoid_icaos.begin(), params.avoid_icaos.end(),
                  e.icao) != params.avoid_icaos.end()) {
      continue;
    }
    // 距离过滤。
    const double distance_nm =
        arr.DistanceTo(bf::Coordinate{e.coord.latitude, e.coord.longitude});
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
  std::vector<AirportEntry> airports;
  airports.reserve(graph.coords.size() - graph.first_airport_vertex);
  for (size_t i = static_cast<size_t>(graph.first_airport_vertex);
       i < graph.coords.size(); ++i) {
    AirportEntry e;
    e.icao = std::string(graph.idents[i].IdentView());
    e.coord = {graph.coords[i].latitude, graph.coords[i].longitude};
    e.elevation_ft = graph.airport_elevations_ft[static_cast<int>(i) -
                                                 graph.first_airport_vertex];
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
  for (const auto& entry : airports_) {
    if (entry.icao == icao) return &entry;
  }
  return nullptr;
}

}  // namespace px
