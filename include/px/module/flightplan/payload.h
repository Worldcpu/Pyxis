// SPDX-License-Identifier: MIT
#pragma once

#include "px/module/flightplan/airframe.h"

namespace px {

// 配载结果（决策 13：pax/cargo → payload/ZFW 单向链；ZFW 直接输入跳过）。
struct PayloadResult {
  int pax_count = 0;
  double cargo_kg = 0.0;
  double payload_kg = 0.0;
  double zfw_kg = 0.0;
  // 双入口可区分（决策 13）：true = ZFW 直接输入跳过配载链；
  // false = 配载计算产生。默认构造的结果两者皆非——消费端据此判空。
  bool from_zfw_input = false;
};

// 配载计算：payload = pax × (unit_pax + unit_bag) + cargo；ZFW = DOW +
// payload。重量/单位重量字段直接取 airframe 档案（同模块同层，不复制）。
PayloadResult ComputePayload(const Airframe& airframe, int pax_count,
                             double cargo_kg);

// ZFW 直接输入（决策 13：给了 zfw_kg 则跳过配载，资深用户/Fenix EFB 同步）。
PayloadResult ComputePayloadFromZfw(double zfw_kg);

}  // namespace px
