// SPDX-License-Identifier: MIT
#pragma once

#include <vector>

namespace px {

// 巡航高度规则（决策 27：三态，auto 由上层按起降场区域推断）。
enum class AltitudeRule {
  kAuto,  // 起降场均在中国十 FIR → kChina，否则 kIcao
  kIcao,  // 半球规则：东行（磁航迹 000-179）奇数 FL、西行偶数 FL
  kChina,  // 中国米制 RVSM：东向/西向分层表（FLAS）
};

// 候选巡航层（决策 8：输出 (FL, 米制等价, 规则名) 三元组——meters 为中国
// 层表内值或 ICAO 层 FL×30.48 近似）。
struct CruiseLevel {
  int fl = 0;      // 百英尺
  int meters = 0;  // 米制等价
};

// 生成候选层集（决策 8/25）：按规则与航向取层 → 升限硬过滤 →
// 高度带 [min_fl, max_fl] 过滤。候选层集 ∩ 高度带保证与路由搜索一致。
// kAuto 为未解析态：返回空（上层须先推断为 kIcao/kChina）；航向负数
// 归一化到 [0, 360)（-90 ≡ 270 西行）。
// 单点带（min_fl == max_fl，决策 25 手动巡航高锁定）：跳过规则层合法性
// 直接返回该单层（决策 8：手动只校验提示不拦截），中国表内命中表值。
// 契约：调用方须保证 min_fl <= max_fl（高度带有效）；FL 非负。
std::vector<CruiseLevel> CandidateLevels(AltitudeRule rule, double track_deg,
                                         int service_ceiling_ft, int min_fl,
                                         int max_fl);

}  // namespace px
