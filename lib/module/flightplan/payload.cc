// SPDX-License-Identifier: MIT
// 配载计算（决策 13：pax/cargo → payload/ZFW 单向链；ZFW 直接输入跳过）。
#include "px/module/flightplan/payload.h"

namespace px {

PayloadResult ComputePayload(const Airframe& airframe, int pax_count,
                             double cargo_kg) {
  PayloadResult result;
  result.pax_count = pax_count;
  result.cargo_kg = cargo_kg;
  result.payload_kg =
      pax_count * (airframe.unit_pax_kg + airframe.unit_bag_kg) + cargo_kg;
  result.zfw_kg = airframe.dow_kg + result.payload_kg;
  return result;
}

PayloadResult ComputePayloadFromZfw(double zfw_kg) {
  PayloadResult result;
  result.zfw_kg = zfw_kg;
  result.from_zfw_input = true;
  return result;
}

}  // namespace px
