// SPDX-License-Identifier: MIT
#include "flightplan_json_module.h"

namespace px {

bool FlightPlanJsonModule::WriteFields(
    rapidjson::Writer<rapidjson::StringBuffer>& writer,
    const JsonContext& ctx) const {
  if (!ctx.flightplan) return false;
  // Phase 8: 序列化 flightplan 字段
  return true;
}

}  // namespace px
