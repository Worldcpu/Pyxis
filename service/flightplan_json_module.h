// SPDX-License-Identifier: MIT
#pragma once
#include "px/service/json_module.h"

namespace px {

class FlightPlanJsonModule : public JsonModule {
 public:
  const char* Name() const override { return "flightplan"; }
  bool WriteFields(rapidjson::Writer<rapidjson::StringBuffer>& writer,
                   const JsonContext& ctx) const override;
};

}  // namespace px
