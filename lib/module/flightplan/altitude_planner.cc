// SPDX-License-Identifier: MIT
// 巡航层规划（决策 8/25/27）：规则层集生成 → 升限硬过滤 → 高度带过滤。
// 中国 RVSM 分层表取自参考实现 FLAS（东向/西向米制层）。
#include "px/module/flightplan/altitude_planner.h"

#include <algorithm>
#include <cmath>

namespace px {

namespace {

// 中国 RVSM 米制层（FLAS 完整表：高空段 + 中低空段；feet 为米制层的
// 英尺近似，换算 FL 显示用）。
struct ChinaLevel {
  int meters;
  int feet;
};

constexpr ChinaLevel kChinaEast[] = {
    {14900, 48900}, {13700, 44900}, {12500, 41100}, {11900, 39100},
    {11300, 37100}, {10700, 35100}, {10100, 33100}, {9500, 31100},
    {8900, 29100},  {8100, 26600},  {7500, 24600},  {6900, 22600},
    {6300, 20700},  {5700, 18700},  {5100, 16700},  {4500, 14800},
    {3900, 12800},  {3300, 10800},  {2700, 8900},   {2100, 6900},
    {1500, 4900},   {900, 3000},
};

constexpr ChinaLevel kChinaWest[] = {
    {14300, 46900}, {13100, 43000}, {12200, 40100}, {11600, 38100},
    {11000, 36100}, {10400, 34100}, {9800, 32100},  {9200, 30100},
    {8400, 27600},  {7800, 25600},  {7200, 23600},  {6600, 21700},
    {6000, 19700},  {5400, 17700},  {4800, 15700},  {4200, 13800},
    {3600, 11800},  {3000, 9800},   {2400, 7900},   {1800, 5900},
    {1200, 3900},   {600, 2000},
};

}  // namespace

std::vector<CruiseLevel> CandidateLevels(AltitudeRule rule, double track_deg,
                                         int service_ceiling_ft, int min_fl,
                                         int max_fl) {
  // 航向归一化到 [0, 360)：fmod(-90, 360) = -90 会被误判东行。
  const double track = std::fmod(std::fmod(track_deg, 360.0) + 360.0, 360.0);
  const bool eastbound = track < 180.0;

  // 单点带短路（决策 25）：手动巡航高 → 高度带联动锁 [FL,FL]——用户覆盖
  // 优先，跳过规则层合法性（决策 8：手动只校验提示不拦截——超限警告走
  // PlanChecks，不在层集拦截）。ICAO/China 一致。米制等价：中国表内层
  // 命中表值，否则 FL×30.48 近似。
  if (min_fl == max_fl) {
    int meters = static_cast<int>(std::lround(min_fl * 30.48));
    if (rule == AltitudeRule::kChina) {
      const auto& table = eastbound ? kChinaEast : kChinaWest;
      for (const auto& level : table) {
        if (static_cast<int>(std::lround(level.feet / 100.0)) == min_fl) {
          meters = level.meters;
          break;
        }
      }
    }
    return {{min_fl, meters}};
  }

  // kAuto 是未解析态（决策 27：上层按起降场区域推断后再调用）——显式空
  // 返回优于静默按 ICAO 处理（调用方缺陷立即可见）。
  if (rule == AltitudeRule::kAuto) return {};

  std::vector<CruiseLevel> levels;

  if (rule == AltitudeRule::kChina) {
    const auto& table = eastbound ? kChinaEast : kChinaWest;
    for (const auto& level : table) {
      const int fl = static_cast<int>(std::lround(level.feet / 100.0));
      if (fl > max_fl || fl > service_ceiling_ft / 100) continue;
      if (fl < min_fl) continue;
      levels.push_back({fl, level.meters});
    }
    // 表为降序（FLAS 惯例），层集统一升序。
    std::reverse(levels.begin(), levels.end());
    return levels;
  }

  // ICAO 半球规则：RVSM 层间隔 1000ft（FL 差 20）——东行层 FL mod 20 == 10
  // （FL250/270/290/…），西行层 FL mod 20 == 0（FL260/280/…）。
  int start = min_fl;
  while (start % 20 != (eastbound ? 10 : 0)) ++start;
  for (int fl = start; fl <= max_fl && fl <= service_ceiling_ft / 100;
       fl += 20) {
    levels.push_back({fl, static_cast<int>(std::lround(fl * 30.48))});
  }
  return levels;
}

}  // namespace px
