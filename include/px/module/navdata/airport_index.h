// SPDX-License-Identifier: MIT
// px_navdata 导航数据只读视图公开头（决策 48：px 自建导航数据模块——
// 不动 bf 源码，链接 bravofinder 库经公开 API 读 bfdb）。
//
// 本模块职责：机场索引（graph 段构建）+ 备降两阶段过滤（距离粗滤 →
// CIFP 跑道精滤）。同时是未来地图航路图的数据基础（graph 段全量航路点
// 坐标/kind/航路名）。
//
// 分层纪律：公开头不暴露 bf 类型（px 层跨命名空间边界一律转换，见
// bf_adapter）——GeoCoord 为 px 自有值类型，bf 转换在 .cc 内完成。
#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include "px/core/result.h"

namespace px {

// 经纬度（度，WGS-84；纬度北正、经度东正）。
struct GeoCoord {
  double latitude = 0.0;
  double longitude = 0.0;
};

// 机场索引条目（由 graph 段机场顶点构建）。
struct AirportEntry {
  std::string icao;
  GeoCoord coord;
  int elevation_ft = 0;
};

// 备降过滤参数（决策 12 修订 2026-08-07：距离上限/排除列表；跑道过滤
// 砍掉——bfdb 无跑道长度数据（Runway 仅阈值坐标），留待数据源补齐）。
struct AlternatesParams {
  double max_distance_nm = 400.0;
  std::vector<std::string> avoid_icaos;
  size_t limit = 5;
};

// 备降候选条目（决策 12/48：无 name——bf 无机场名称数据）。
struct AlternateCandidate {
  std::string icao;
  double distance_nm = 0.0;
  std::string route = "DCT";  // 决策 12：默认 DCT 大圆
};

// 大圆距离（NM，球面 haversine）——薄包装 bf::Coordinate::DistanceTo。
double DistanceNm(const GeoCoord& a, const GeoCoord& b) noexcept;

// 备降过滤纯函数（决策 12 修订 2026-08-07）：4 字 ICAO（排除 FAA LID 等
// 短码）→ 排除列表剔除 → 距离 ≤ max_distance_nm → 距离升序 → 截断 limit 条。
std::vector<AlternateCandidate> FilterAlternates(
    std::span<const AirportEntry> airports, const GeoCoord& arrival,
    const AlternatesParams& params);

// 机场索引（决策 48）：从 .bfdb 的 graph 段读取全部机场顶点构建。
// 不可变值类型；longest_runway_ft 均为 0（跑道在 CIFP 惰性段，
// 由调用方按需精滤——两阶段过滤的距离粗滤在此索引上完成）。
class AirportIndex {
 public:
  // 读取 .bfdb 并构建机场索引。失败：文件缺失 kDataMissing /
  // 损坏（bad magic/截断）kCacheCorrupt / 无机场顶点 kInternalError。
  static Result<AirportIndex> Open(const std::string& bfdb_path);

  // 全机场条目（graph 顶点顺序，距离粗滤遍历用）。
  std::span<const AirportEntry> airports() const noexcept { return airports_; }

  // 按 ICAO 查找（到达场坐标查询用）；未找到返回 nullptr。
  const AirportEntry* Find(const std::string& icao) const noexcept;

 private:
  std::vector<AirportEntry> airports_;
};

}  // namespace px
