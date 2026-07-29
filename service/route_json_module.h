// SPDX-License-Identifier: MIT
#pragma once
#include "px/service/json_module.h"

namespace px {

class RouteJsonModule : public JsonModule {
 public:
  const char* Name() const override { return "route"; }
  bool WriteFields(rapidjson::Writer<rapidjson::StringBuffer>& writer,
                   const JsonContext& ctx) const override;
};

}  // namespace px
