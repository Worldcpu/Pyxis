// SPDX-License-Identifier: MIT
#include "route_json_module.h"

#include "core/routing/route_json.h"  // bf::WriteRouteJson

namespace px {

bool RouteJsonModule::WriteFields(
    rapidjson::Writer<rapidjson::StringBuffer>& writer,
    const JsonContext& ctx) const {
  if (!ctx.route) return false;
  writer.Key(Name());
  bf::WriteRouteJson(writer, *ctx.route);
  return true;
}

}  // namespace px
