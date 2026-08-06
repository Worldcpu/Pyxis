// SPDX-License-Identifier: MIT
#pragma once

#include "px/module/flightplan/flight_plan.h"

namespace bf {
struct Route;  // 仅前向声明——px API 面不暴露 bf 类型（CLAUDE.md 纪律）
}  // namespace bf

namespace px {

// bf::Route → FlightPlan 一次性转换（决策 2b：独立值类型在 FromBf 层
// 完成映射；段序列与点序列两视图均派生自 bf::Route，互不推导）。
FlightPlan FromBf(const bf::Route& route);

}  // namespace px
