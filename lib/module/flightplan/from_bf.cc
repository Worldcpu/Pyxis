// SPDX-License-Identifier: MIT
// FromBf 转换实现：bf::Route → FlightPlan（决策 2b——段序列在转换层
// 一次性派生；程序聚合段规则见决策 2c）。
#include "px/module/flightplan/from_bf.h"

#include "core/domain/nav_tokens.h"
#include "core/routing/route.h"

namespace px {

namespace {

// 判断并返回 leg 的程序段类型（SID/STAR），否则 kEnroute。
// 连接 token 复用 bf 命名常量（nav_tokens.h），防止笔误静默误分类。
SegmentKind LegKind(const std::string& via) {
  if (via == bf::kSidToken) return SegmentKind::kSid;
  if (via == bf::kStarToken) return SegmentKind::kStar;
  return SegmentKind::kEnroute;
}

// 程序聚合段判定：SID/STAR/进近连续 legs 聚合为一段（决策 2c）。
bool IsProgramSegment(SegmentKind kind) {
  return kind == SegmentKind::kSid || kind == SegmentKind::kStar ||
         kind == SegmentKind::kApproach;
}

// 到达侧尾部连续 DCT 起点（决策 2c）：无 STAR（kTerminalTransition）时，
// 从尾部最后一个非 DCT leg 之后起，全部连续 DCT leg 归属 kApproach
// （IAF 过渡 + 落地，语义同一进近段）。全 DCT 航路防御：出发侧 DCT
// 不是进近——只保留最后一段。
size_t ApproachStart(const bf::Route& route) {
  if (route.arr_connection != bf::ConnectionKind::kTerminalTransition) {
    return route.legs.size();
  }
  size_t start = route.legs.size();
  while (start > 0 && route.legs[start - 1].via == bf::kDctToken) --start;
  // 全 DCT 航路防御：出发侧 DCT 不是进近——只保留最后一段。
  // 空 legs 时保持 size()（0）——`size() - 1` 会下溢为 SIZE_MAX。
  if (start == 0 && !route.legs.empty()) start = route.legs.size() - 1;
  return start;
}

// 连续同程序 legs 聚合：更新末段 to_ident 并累加距离。
void AppendToProgramSegment(FlightSegment& segment, const bf::RouteLeg& leg) {
  segment.to_ident = leg.to;
  segment.distance_nm += leg.distance_nm;
}

}  // namespace

FlightPlan FromBf(const bf::Route& route) {
  FlightPlan plan;
  plan.route_string = route.route_string;
  // 候选契约字段（决策 9）搬运：程序名/跑道 + bf 连接 token。
  plan.sid = route.sid;
  plan.star = route.star;
  plan.dep_runway = route.dep_runway;
  plan.arr_runway = route.arr_runway;
  plan.dep_connection = bf::ToString(route.dep_connection);
  plan.arr_connection = bf::ToString(route.arr_connection);

  const size_t approach_start = ApproachStart(route);
  for (size_t i = 0; i < route.legs.size(); ++i) {
    const auto& leg = route.legs[i];
    SegmentKind kind = LegKind(leg.via);
    if (kind == SegmentKind::kEnroute && i >= approach_start) {
      kind = SegmentKind::kApproach;
    }
    if (IsProgramSegment(kind) && !plan.segments.empty() &&
        plan.segments.back().kind == kind) {
      AppendToProgramSegment(plan.segments.back(), leg);
      continue;
    }
    plan.segments.push_back({kind, leg.from, leg.to, leg.via, leg.distance_nm});
  }

  // 点序列派生：含程序内点；segment_index 按段游标推进（点到达当前段
  // 的 to_ident 且非最后点 → 进入下一段；最后点（落地机场）归属末段）。
  // segments 为空（单顶点路径）时游标 = -1 哨兵，点无段归属。
  int seg = plan.segments.empty() ? -1 : 0;
  std::string via =
      plan.segments.empty() ? std::string{} : plan.segments[0].via;
  for (size_t i = 0; i < route.points.size(); ++i) {
    const auto& p = route.points[i];
    // to 点本身仍属当前段，推进作用于下一轮（决策 30③：落地机场点
    // 归属末段，故最后点不推进）。
    plan.points.push_back(
        {p.ident, via, p.coord.latitude, p.coord.longitude, seg});
    if (seg >= 0 && i > 0 && seg + 1 < static_cast<int>(plan.segments.size()) &&
        p.ident == plan.segments[seg].to_ident) {
      ++seg;
      via = plan.segments[seg].via;
    }
  }
  return plan;
}

}  // namespace px
