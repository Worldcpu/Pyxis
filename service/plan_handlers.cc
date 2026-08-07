// SPDX-License-Identifier: MIT
// plan 流程 handler 实现（决策 15：px 直调 NavDatabase 按 px 形状渲染；
// 参数校验语义对齐 bf FindRoutesHandler——k 边界/高度带/level 兜底）。
#include "px/service/plan_handlers.h"

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <algorithm>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "bf_adapter.h"
#include "core/domain/coordinate.h"
#include "core/routing/route_request.h"
#include "handlers.h"  // bf::service 常量（kMaxFl/kMaxK）
#include "io/nav_database.h"
#include "px/module/flightplan/altitude_planner.h"
#include "px/module/flightplan/from_bf.h"
#include "px/module/flightplan/payload.h"
#include "px/service/airframe_json.h"
#include "px/service/bf_rpc.h"
#include "px/service/plan_json.h"
#include "px/service/pln_export.h"
#include "px/service/profile_json.h"

namespace px {

namespace {

// px::ErrorCode → JSON-RPC 错误码（决策 18 分区：400 参数/404 查无/
// 422 无解/-32000 内部）。
int ErrorToRpcCode(ErrorCode code) {
  switch (code) {
    case ErrorCode::kInvalidInput:
    case ErrorCode::kInvalidArgument:
    case ErrorCode::kParseError:  // 手写 route_string 解析失败 = 参数错
      return 400;
    case ErrorCode::kNotFound:
      return 404;
    case ErrorCode::kNoRouteFound:
      return 422;
    default:
      return -32000;
  }
}

RpcResult RpcError(int code, std::string message) {
  return {false, "", code, std::move(message)};
}

RpcResult RpcError(ErrorCode code, std::string message) {
  return RpcError(ErrorToRpcCode(code), std::move(message));
}

RpcResult RpcError(const Error& error) {
  return RpcError(ErrorToRpcCode(error.code), error.message);
}

// navdata 缺失统一错误（决策 47：服务照起，查询返回 -32000）。
RpcResult NoDatabase() {
  return RpcError(-32000, "导航数据不可用（--navdata-dir 缺失或加载失败）");
}

// alternates 候选 JSON 渲染（决策 48：{icao, distance_nm, route}；
// 无 name——bf 无机场名称数据；无跑道——数据源缺失，决策 12 修订）。
std::string RenderAlternatesJson(
    const std::vector<AlternateCandidate>& candidates) {
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  writer.StartArray();
  for (const auto& c : candidates) {
    writer.StartObject();
    writer.Key("icao");
    writer.String(c.icao.c_str());
    writer.Key("distance_nm");
    writer.Double(c.distance_nm);
    // 坐标（审查修复：前端备降图层从此取数，此前 route 字符串无法定位）。
    writer.Key("lat");
    writer.Double(c.coord.latitude);
    writer.Key("lon");
    writer.Double(c.coord.longitude);
    writer.Key("route");
    writer.String(c.route.c_str());
    writer.EndObject();
  }
  writer.EndArray();
  return buffer.GetString();
}

// 可选字符串参数：缺省不报错；present 非 string → 400。
bool OptString(const rapidjson::Value& params, const char* key,
               std::string* out) {
  if (!params.HasMember(key)) return true;
  if (!params[key].IsString()) return false;
  *out = params[key].GetString();
  return true;
}

// 可选字符串数组（id 列表）：present 须数组且元素全 string（≤256，对齐
// bf kMaxIdListSize 防护）。
bool OptIdList(const rapidjson::Value& params, const char* key,
               std::vector<std::string>* out) {
  if (!params.HasMember(key)) return true;
  if (!params[key].IsArray() || params[key].Size() > 256) return false;
  out->clear();
  for (const auto& id : params[key].GetArray()) {
    if (!id.IsString()) return false;
    out->push_back(id.GetString());
  }
  return true;
}

// plan.analyze（决策 54：航路合法性检查——bf ParseRoute 语义解析：
// 起降场/航路点/航路存在性 + 距离全量校验，错误消息可直接作前端提示）。
RpcResult HandleAnalyze(const rapidjson::Value& params,
                        const PlanContext& ctx) {
  if (!params.HasMember("route_string") || !params["route_string"].IsString()) {
    return RpcError(400, "route_string 必填（决策 54 统一输入）");
  }
  if (ctx.db == nullptr) return NoDatabase();
  const auto parsed = ctx.db->ParseRoute(params["route_string"].GetString());
  AnalyzeResult result;
  result.cycle = static_cast<int>(ctx.cycle);
  if (parsed.has_value()) {
    result.valid = true;
    result.distance_nm = parsed.value().total_distance_nm;
  } else {
    const auto& error = parsed.error();
    // 语义错误（点/航路不存在等）→ 逐条展示；其他（如数据库态）透传。
    if (error.code != bf::ErrorCode::kRouteParseError) {
      return RpcError(FromBfError(error));
    }
    result.valid = false;
    result.errors.push_back(error.message);
  }
  return {true, RenderAnalyzeJson(result), 0, ""};
}

// plan.routes（决策 7/9：10 字段 → bf FindRoutes → px 候选形状）。
RpcResult HandleRoutes(const rapidjson::Value& params, const PlanContext& ctx) {
  if (!params.HasMember("departure") || !params["departure"].IsString() ||
      !params.HasMember("arrival") || !params["arrival"].IsString()) {
    return RpcError(400, "departure 和 arrival 必填");
  }
  const std::string departure = params["departure"].GetString();
  const std::string arrival = params["arrival"].GetString();
  if (departure == arrival) {
    return RpcError(422, "起降机场不能相同");
  }

  bf::RouteRequest request;
  request.departure = departure;
  request.arrival = arrival;

  // 高度带（决策 7 默认 FL250-410 保证 MORA；bf 校验语义：min≤max、
  // 非负、≤kMaxFl；单侧给定时另一侧默认同值；present 非 int 拒绝——
  // 审查修复：此前类型错静默回退默认带）。
  int min_fl = 250, max_fl = 410;
  const bool has_min = params.HasMember("min_fl");
  const bool has_max = params.HasMember("max_fl");
  if (has_min && !params["min_fl"].IsInt()) {
    return RpcError(400, "min_fl 必须为整数");
  }
  if (has_max && !params["max_fl"].IsInt()) {
    return RpcError(400, "max_fl 必须为整数");
  }
  if (has_min || has_max) {
    min_fl = has_min ? params["min_fl"].GetInt() : params["max_fl"].GetInt();
    max_fl = has_max ? params["max_fl"].GetInt() : params["min_fl"].GetInt();
    if (min_fl > max_fl) return RpcError(400, "min_fl 不得超过 max_fl");
    if (min_fl < 0) return RpcError(400, "min_fl/max_fl 必须非负");
    if (max_fl > bf::service::kMaxFl) {
      return RpcError(400, "max_fl 不得超过 600");
    }
  }
  request.altitude = bf::FlRange{min_fl, max_fl};

  // 高低空偏好（决策 7；未知值静默回退默认——bf 语义）。
  if (params.HasMember("level") && params["level"].IsString()) {
    const std::string_view level = params["level"].GetString();
    if (level == "low") {
      request.level = bf::LevelPreference::kLow;
    } else if (level == "high") {
      request.level = bf::LevelPreference::kHigh;
    }
  }

  // k（决策 7 默认 5；present 须 int，1..kMaxK——bf 服务端防护语义）。
  int k = 5;
  if (params.HasMember("k")) {
    if (!params["k"].IsInt()) return RpcError(400, "k 必须为整数");
    k = params["k"].GetInt();
    if (k < 1) return RpcError(400, "k 必须为正整数（≥ 1）");
    if (k > bf::service::kMaxK) return RpcError(400, "k 不得超过 15");
  }
  request.k = k;

  if (!OptString(params, "departure_runway", &request.departure_runway) ||
      !OptString(params, "arrival_runway", &request.arrival_runway) ||
      !OptString(params, "departure_sid", &request.departure_sid) ||
      !OptString(params, "arrival_star", &request.arrival_star)) {
    return RpcError(400, "跑道/SID/STAR 必须为字符串");
  }
  if (!OptIdList(params, "avoid_waypoints", &request.avoid_waypoints) ||
      !OptIdList(params, "forced_points", &request.forced_points)) {
    return RpcError(400,
                    "avoid_waypoints/forced_points 须为字符串数组（≤256）");
  }

  // seed（决策 7/契约 2：换一批 = seed+1；不带 = 确定性 k 最短路）。
  uint32_t seed = 0;
  if (params.HasMember("random_seed")) {
    if (!params["random_seed"].IsUint()) {
      return RpcError(400, "random_seed 必须为无符号整数");
    }
    seed = params["random_seed"].GetUint();
    request.random_seed = seed;
  }
  // TODO(Phase 10)：airway_rules 复杂形状解析对齐 bf ParseAirwayRules——
  // present 时拒绝（不静默丢意图）。
  if (params.HasMember("airway_rules")) {
    return RpcError(400, "airway_rules 暂不支持（Phase 10）");
  }

  if (ctx.db == nullptr) return NoDatabase();
  auto result = ctx.db->FindRoutes(request);
  if (!result.has_value()) {
    return RpcError(FromBfError(result.error()));
  }
  const auto& routes = result.value();
  std::vector<FlightPlan> candidates;
  candidates.reserve(routes.size());
  for (const auto& route : routes) {
    candidates.push_back(FromBf(route));
  }
  return {true, RenderPlanCandidatesJson(candidates, seed), 0, ""};
}

// plan.alternates（决策 12/48：px_navdata 两阶段过滤）。
RpcResult HandleAlternates(const rapidjson::Value& params,
                           const PlanContext& ctx) {
  if (!params.HasMember("arrival") || !params["arrival"].IsString()) {
    return RpcError(400, "arrival 必填");
  }
  AlternatesParams alt_params;
  if (params.HasMember("max_distance_nm")) {
    if (!params["max_distance_nm"].IsNumber()) {
      return RpcError(400, "max_distance_nm 必须为数值");
    }
    alt_params.max_distance_nm = params["max_distance_nm"].GetDouble();
    if (alt_params.max_distance_nm <= 0.0) {
      return RpcError(400, "max_distance_nm 必须为正");
    }
  }
  if (!OptIdList(params, "avoid_icaos", &alt_params.avoid_icaos)) {
    return RpcError(400, "avoid_icaos 须为字符串数组（≤256）");
  }
  if (ctx.airports == nullptr) return NoDatabase();
  const std::string arrival = params["arrival"].GetString();
  const AirportEntry* arrival_entry = ctx.airports->Find(arrival);
  if (arrival_entry == nullptr) {
    return RpcError(404, "到达机场不在导航数据中");
  }

  // 决策 12 修订：距离 + 排除 + 4 字 ICAO（跑道过滤砍掉——无数据源）；
  // 到达场自身排除（0 NM 假候选，审查修复；用索引条目规范大写）。
  alt_params.exclude_icao = arrival_entry->icao;
  const auto candidates = FilterAlternates(ctx.airports->airports(),
                                           arrival_entry->coord, alt_params);
  return {true, RenderAlternatesJson(candidates), 0, ""};
}

// plan.generate（决策 8/14/23/26/45 + 契约 6：编排已有域层组件——
// ParseRoute/FromBf/高度规划/配载/校验；燃油字段位 Phase 10 填充）。
RpcResult HandleGenerate(const rapidjson::Value& params,
                         const PlanContext& ctx) {
  if (!params.HasMember("route_string") || !params["route_string"].IsString()) {
    return RpcError(400, "route_string 必填（决策 23 统一输入）");
  }
  if (!params.HasMember("airframe") || !params["airframe"].IsObject()) {
    return RpcError(400, "airframe 必填");
  }
  Airframe airframe;
  if (!ParseAirframe(params["airframe"], &airframe)) {
    return RpcError(400, "airframe 形状非法");
  }
  const auto issues = ValidateAirframe(airframe);
  if (!issues.empty()) {
    return RpcError(400, "airframe 校验失败: " + issues.front().field + " " +
                             issues.front().message);
  }
  // 配载（决策 13 双入口互斥；pax/cargo 任一缺省 0）。
  const bool has_pax =
      params.HasMember("pax_count") || params.HasMember("cargo_kg");
  const bool has_zfw = params.HasMember("zfw_kg");
  if (has_pax && has_zfw) {
    return RpcError(400, "pax_count/cargo_kg 与 zfw_kg 互斥");
  }
  // 无任何配载输入 → 400（缺省 0 会产出 ZFW=0 的物理不可能计划——审查修复）。
  if (!has_pax && !has_zfw) {
    return RpcError(400, "配载参数必填（pax_count/cargo_kg 或 zfw_kg）");
  }
  PayloadResult payload;
  if (has_zfw) {
    if (!params["zfw_kg"].IsNumber()) return RpcError(400, "zfw_kg 必须为数值");
    // 值校验（审查修复：此前负数/低于 DOW 的物理不可能值静默通过——
    // ZFW = DOW + 业载，必 ≥ DOW）。
    const double zfw = params["zfw_kg"].GetDouble();
    if (zfw < 0.0 || zfw < airframe.dow_kg) {
      return RpcError(400, "zfw_kg 必须 ≥ 0 且 ≥ DOW（零油重 = DOW + 业载）");
    }
    payload = ComputePayloadFromZfw(zfw);
  } else if (has_pax) {
    int pax = 0;
    double cargo = 0.0;
    if (params.HasMember("pax_count")) {
      if (!params["pax_count"].IsInt() || params["pax_count"].GetInt() < 0) {
        return RpcError(400, "pax_count 必须为非负整数");
      }
      pax = params["pax_count"].GetInt();
    }
    if (params.HasMember("cargo_kg")) {
      if (!params["cargo_kg"].IsNumber() ||
          params["cargo_kg"].GetDouble() < 0) {
        return RpcError(400, "cargo_kg 必须为非负数");
      }
      cargo = params["cargo_kg"].GetDouble();
    }
    payload = ComputePayload(airframe, pax, cargo);
  }
  // 巡航高度（决策 8/25：手动 cruise_fl 或 auto 候选层）。
  const bool has_cruise = params.HasMember("cruise_fl");
  if (has_cruise) {
    if (!params["cruise_fl"].IsInt())
      return RpcError(400, "cruise_fl 必须为整数");
    const int fl = params["cruise_fl"].GetInt();
    if (fl < 0 || fl > bf::service::kMaxFl) {
      return RpcError(400, "cruise_fl 越界（0..600）");
    }
  }
  // 规则三态（决策 27）。
  AltitudeRule rule = AltitudeRule::kAuto;
  if (params.HasMember("altitude_rule")) {
    if (!params["altitude_rule"].IsString()) {
      return RpcError(400, "altitude_rule 必须为字符串");
    }
    const std::string_view s = params["altitude_rule"].GetString();
    if (s == "icao") {
      rule = AltitudeRule::kIcao;
    } else if (s == "china") {
      rule = AltitudeRule::kChina;
    } else if (s == "auto") {
      rule = AltitudeRule::kAuto;
    } else {
      return RpcError(400, "altitude_rule 仅支持 auto/icao/china");
    }
  }
  // 候选标记（决策 26 修订：mora_checked 来源——候选搜索已带 MORA）。
  bool candidate = false;
  if (params.HasMember("candidate")) {
    if (!params["candidate"].IsBool())
      return RpcError(400, "candidate 必须为布尔");
    candidate = params["candidate"].GetBool();
  }
  // auto 高度带（决策 25；校验对齐 plan.routes——审查修复：此前在
  // ParseRoute 后校验会被 db 缺失短路，且缺 min≤max/非负/≤600 检查）。
  int min_fl = 250, max_fl = 410;
  if (params.HasMember("min_fl")) {
    if (!params["min_fl"].IsInt()) return RpcError(400, "min_fl 必须为整数");
    min_fl = params["min_fl"].GetInt();
  }
  if (params.HasMember("max_fl")) {
    if (!params["max_fl"].IsInt()) return RpcError(400, "max_fl 必须为整数");
    max_fl = params["max_fl"].GetInt();
  }
  if (min_fl > max_fl) return RpcError(400, "min_fl 不得超过 max_fl");
  if (min_fl < 0) return RpcError(400, "min_fl/max_fl 必须非负");
  if (max_fl > bf::service::kMaxFl) {
    return RpcError(400, "max_fl 不得超过 600");
  }
  // 五字段（决策 45 回显）。
  std::string callsign, etd, alternate;
  if (!OptString(params, "callsign", &callsign) ||
      !OptString(params, "etd", &etd) ||
      !OptString(params, "alternate", &alternate)) {
    return RpcError(400, "callsign/etd/alternate 必须为字符串");
  }
  // seed 参数名统一 random_seed（与 plan.routes 一致——审查修复：此前
  // 前端发 random_seed 被静默忽略，决策 7 seed 回传/复现契约断链）。
  uint32_t seed = 0;
  if (params.HasMember("random_seed")) {
    if (!params["random_seed"].IsUint()) {
      return RpcError(400, "random_seed 必须为无符号整数");
    }
    seed = params["random_seed"].GetUint();
  }

  if (ctx.db == nullptr) return NoDatabase();
  auto parsed = ctx.db->ParseRoute(params["route_string"].GetString());
  if (!parsed.has_value()) {
    return RpcError(FromBfError(parsed.error()));  // kRouteParseError → 400
  }
  FlightPlan plan = FromBf(parsed.value());
  // 高度填充。
  if (has_cruise) {
    const int fl = params["cruise_fl"].GetInt();
    // 米制等价统一 30.48 换算（对齐 CandidateLevels——审查修复 fl*30）。
    plan.altitude = {fl, static_cast<int>(std::lround(fl * 30.48)),
                     rule == AltitudeRule::kAuto ? AltitudeRule::kIcao : rule,
                     "手动", true};
  } else {
    // auto：候选层集 ∩ 高度带（决策 25）；rule auto → ICAO 半球档
    // （中国 FIR 推断 TODO Phase 10——决策 27 修订）。
    const AltitudeRule effective =
        rule == AltitudeRule::kAuto ? AltitudeRule::kIcao : rule;
    double track_deg = 0.0;
    if (plan.points.size() >= 2) {
      const auto& a = plan.points.front();
      const auto& b = plan.points.back();
      track_deg = bf::Coordinate{a.latitude, a.longitude}.BearingTo(
          bf::Coordinate{b.latitude, b.longitude});
    }
    // min_fl/max_fl 已在参数区解析并校验（上方）——此处直接使用。
    const auto levels = CandidateLevels(
        effective, track_deg, static_cast<int>(airframe.service_ceiling_ft),
        min_fl, max_fl);
    if (levels.empty()) {
      return RpcError(422, "无可用巡航层（高度带内无候选层）");
    }
    plan.altitude = {levels.front().fl, levels.front().meters, effective,
                     "自动（无风降级，Phase 9）", false};
  }
  // 配载与重量（决策 13；TOW/LW 由 Phase 10 燃油引擎填充）。
  plan.weights.dow_kg = airframe.dow_kg;
  plan.weights.zfw_kg = payload.zfw_kg;
  plan.weights.mzfw_kg = airframe.mzfw_kg;
  plan.weights.mtow_kg = airframe.mtow_kg;
  plan.weights.mlw_kg = airframe.mlw_kg;
  // 检查（决策 8：超限警告；Phase 9 最小集）。
  if (payload.zfw_kg > airframe.mzfw_kg) {
    plan.checks.status = CheckStatus::kWarning;
    plan.checks.warnings.push_back("ZFW 超过 MZFW");
  }
  // 五字段（决策 45）+ mora_checked（决策 26 修订：候选标记）。
  plan.callsign = std::move(callsign);
  plan.etd = std::move(etd);
  plan.alternate = std::move(alternate);
  plan.wind_source = "none";
  plan.seed = seed;
  plan.mora_checked = candidate;
  return {true, RenderPlanJson(plan), 0, ""};
}

// plan.export（决策 17：前端回传 FlightPlan JSON + format → .PLN XML；
// 无 db 依赖——坐标/高度均在回传 JSON 内）。
RpcResult HandleExport(const rapidjson::Value& params, const PlanContext& ctx) {
  (void)ctx;
  if (!params.HasMember("format") || !params["format"].IsString()) {
    return RpcError(400, "format 必填");
  }
  const std::string format = params["format"].GetString();
  if (format != "msfs2024") {
    return RpcError(400, "format 仅支持 msfs2024（多格式抽象 Phase 12）");
  }
  if (!params.HasMember("flightplan") || !params["flightplan"].IsObject()) {
    return RpcError(400, "flightplan 必填（前端回传 generate 响应 JSON）");
  }
  const auto& fp = params["flightplan"];
  if (!fp.HasMember("route") || !fp["route"].IsObject() ||
      !fp["route"].HasMember("points") || !fp["route"]["points"].IsArray()) {
    return RpcError(400, "flightplan.route.points 缺失");
  }
  const auto& points = fp["route"]["points"];
  if (points.Size() < 2) {
    return RpcError(400, "flightplan.route.points 至少 2 个点");
  }
  // 逐点校验（含中间点——缺失字段/类型错在 rapidjson 上是断言崩溃，
  // 必须全部先查后取）。
  for (rapidjson::SizeType i = 0; i < points.Size(); ++i) {
    const auto& point = points[i];
    if (!point.IsObject() || !point.HasMember("ident") ||
        !point["ident"].IsString() || !point.HasMember("lat") ||
        !point["lat"].IsNumber() || !point.HasMember("lon") ||
        !point["lon"].IsNumber()) {
      return RpcError(400, "flightplan.route.points 元素须含 ident/lat/lon");
    }
    // ident 字符集（审查修复：服务端文件名拼接来源——Phase 10 Tauri
    // 写盘前必须先收口，防路径穿越；[A-Z0-9] 1-8 位覆盖机场/航路点）。
    const std::string_view ident = point["ident"].GetString();
    if (ident.empty() || ident.size() > 8) {
      return RpcError(400, "ident 长度须为 1-8 字符");
    }
    for (const char ch : ident) {
      if (!std::isalnum(static_cast<unsigned char>(ch))) {
        return RpcError(400, "ident 仅允许字母数字");
      }
    }
  }
  const auto& first = points[0];
  const auto& last = points[points.Size() - 1];
  // 巡航高度必填（审查修复：此前缺失默认 0 → .PLN 地面高度，MSFS 按
  // 地面加载——与其余字段的严格校验口径不一致）。
  int cruise_fl = 0;
  if (!fp.HasMember("altitude") || !fp["altitude"].IsObject() ||
      !fp["altitude"].HasMember("fl") || !fp["altitude"]["fl"].IsInt()) {
    return RpcError(400, "flightplan.altitude.fl 必填（整数）");
  }
  cruise_fl = fp["altitude"]["fl"].GetInt();
  PlnExportParams export_params;
  export_params.title = "Pyxis Flight Plan";
  export_params.fp_type = "IFR";
  export_params.cruising_altitude_ft = cruise_fl * 100.0;
  export_params.departure_id = first["ident"].GetString();
  export_params.destination_id = last["ident"].GetString();
  export_params.dep_lat = first["lat"].GetDouble();
  export_params.dep_lon = first["lon"].GetDouble();
  export_params.dest_lat = last["lat"].GetDouble();
  export_params.dest_lon = last["lon"].GetDouble();
  // 中间点（RenderPlnXml 契约：points 不含起降场）。
  std::vector<FlightPoint> mid_points;
  mid_points.reserve(points.Size() - 2);
  for (rapidjson::SizeType i = 1; i + 1 < points.Size(); ++i) {
    FlightPoint point;
    point.ident = points[i]["ident"].GetString();
    point.latitude = points[i]["lat"].GetDouble();
    point.longitude = points[i]["lon"].GetDouble();
    mid_points.push_back(std::move(point));
  }
  const std::string xml = RenderPlnXml(export_params, mid_points);
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  writer.StartObject();
  writer.Key("format");
  writer.String("msfs2024");
  writer.Key("filename");
  writer.String(("pyxis-" + export_params.departure_id + "-" +
                 export_params.destination_id + ".pln")
                    .c_str());
  writer.Key("content");
  writer.String(xml.c_str());
  writer.EndObject();
  return {true, buffer.GetString(), 0, ""};
}

// ---- airframe 四端点（决策 21：data_dir/airframes.json 持久层）----

std::string AirframesPath(const PlanContext& ctx) {
  return ctx.data_dir + "/airframes.json";
}

std::string RenderAirframeJson(const Airframe& airframe) {
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  WriteAirframeJson(writer, airframe);
  return buffer.GetString();
}

RpcResult HandleAirframeList(const rapidjson::Value&, const PlanContext& ctx) {
  if (ctx.data_dir.empty()) return RpcError(-32000, "data_dir 未配置");
  std::lock_guard<std::mutex> lock(*ctx.file_mutex);  // 与写互斥（审查修复）
  const auto airframes = LoadAirframes(AirframesPath(ctx));
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  writer.StartArray();
  for (const auto& airframe : airframes) {
    WriteAirframeJson(writer, airframe);
  }
  writer.EndArray();
  return {true, buffer.GetString(), 0, ""};
}

bool GetAirframeKey(const rapidjson::Value& params, std::string* type,
                    std::string* variant) {
  if (!params.HasMember("type") || !params["type"].IsString() ||
      !params.HasMember("variant") || !params["variant"].IsString()) {
    return false;
  }
  *type = params["type"].GetString();
  *variant = params["variant"].GetString();
  return true;
}

RpcResult HandleAirframeGet(const rapidjson::Value& params,
                            const PlanContext& ctx) {
  if (ctx.data_dir.empty()) return RpcError(-32000, "data_dir 未配置");
  std::string type, variant;
  if (!GetAirframeKey(params, &type, &variant)) {
    return RpcError(400, "type/variant 必填");
  }
  std::lock_guard<std::mutex> lock(*ctx.file_mutex);  // 与写互斥（审查修复）
  for (const auto& airframe : LoadAirframes(AirframesPath(ctx))) {
    if (airframe.type == type && airframe.variant == variant) {
      return {true, RenderAirframeJson(airframe), 0, ""};
    }
  }
  return RpcError(404, "airframe 不存在");
}

RpcResult HandleAirframeUpsert(const rapidjson::Value& params,
                               const PlanContext& ctx) {
  if (ctx.data_dir.empty()) return RpcError(-32000, "data_dir 未配置");
  if (!params.HasMember("airframe") || !params["airframe"].IsObject()) {
    return RpcError(400, "airframe 必填");
  }
  Airframe incoming;
  if (!ParseAirframe(params["airframe"], &incoming)) {
    return RpcError(400, "airframe 形状非法");
  }
  const auto issues = ValidateAirframe(incoming);
  if (!issues.empty()) {
    return RpcError(400, "airframe 校验失败: " + issues.front().field + " " +
                             issues.front().message);
  }
  std::lock_guard<std::mutex> lock(*ctx.file_mutex);  // 决策 44 串行化
  auto airframes = LoadAirframes(AirframesPath(ctx));
  bool replaced = false;
  for (auto& airframe : airframes) {
    if (airframe.type == incoming.type &&
        airframe.variant == incoming.variant) {
      airframe = incoming;
      replaced = true;
      break;
    }
  }
  if (!replaced) airframes.push_back(incoming);
  auto stored = StoreAirframes(AirframesPath(ctx), airframes);
  if (!stored.has_value()) {
    return RpcError(-32000, stored.error().message);
  }
  return {true, RenderAirframeJson(incoming), 0, ""};
}

RpcResult HandleAirframeDelete(const rapidjson::Value& params,
                               const PlanContext& ctx) {
  if (ctx.data_dir.empty()) return RpcError(-32000, "data_dir 未配置");
  std::string type, variant;
  if (!GetAirframeKey(params, &type, &variant)) {
    return RpcError(400, "type/variant 必填");
  }
  std::lock_guard<std::mutex> lock(*ctx.file_mutex);  // 决策 44 串行化
  auto airframes = LoadAirframes(AirframesPath(ctx));
  bool removed = false;
  airframes.erase(std::remove_if(airframes.begin(), airframes.end(),
                                 [&](const Airframe& airframe) {
                                   if (airframe.type == type &&
                                       airframe.variant == variant) {
                                     removed = true;
                                     return true;
                                   }
                                   return false;
                                 }),
                  airframes.end());
  auto stored = StoreAirframes(AirframesPath(ctx), airframes);
  if (!stored.has_value()) {
    return RpcError(-32000, stored.error().message);
  }
  if (!removed) return RpcError(404, "airframe 不存在");
  // 手写 JSON 字面量违反 Writer 纪律（CLAUDE.md）——审查修复。
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  writer.StartObject();
  writer.Key("ok");
  writer.Bool(true);
  writer.EndObject();
  return {true, buffer.GetString(), 0, ""};
}

// ---- profile 四端点（决策 55：data_dir/profiles.json 持久层，airframe
// 同款模式——Suggest Route 偏好） ----

std::string ProfilesPath(const PlanContext& ctx) {
  return ctx.data_dir + "/profiles.json";
}

std::string RenderProfileJson(const Profile& profile) {
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  WriteProfileJson(writer, profile);
  return buffer.GetString();
}

// 校验（决策 55：name 非空 ≤32；k 1..15；min≤max；level low/high）。
const char* ValidateProfile(const Profile& profile) {
  if (profile.name.empty() || profile.name.size() > 32) {
    return "name 须为非空且 ≤32 字符";
  }
  if (profile.k.has_value() && (*profile.k < 1 || *profile.k > 15)) {
    return "k 须在 1..15";
  }
  if (profile.level.has_value() && *profile.level != "low" &&
      *profile.level != "high") {
    return "level 仅支持 low/high";
  }
  if (profile.min_fl.has_value() && profile.max_fl.has_value() &&
      *profile.min_fl > *profile.max_fl) {
    return "min_fl 不得超过 max_fl";
  }
  return nullptr;
}

RpcResult HandleProfileList(const rapidjson::Value&, const PlanContext& ctx) {
  if (ctx.data_dir.empty()) return RpcError(-32000, "data_dir 未配置");
  std::lock_guard<std::mutex> lock(*ctx.file_mutex);
  const auto profiles = LoadProfiles(ProfilesPath(ctx));
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  writer.StartArray();
  for (const auto& profile : profiles) {
    WriteProfileJson(writer, profile);
  }
  writer.EndArray();
  return {true, buffer.GetString(), 0, ""};
}

RpcResult HandleProfileGet(const rapidjson::Value& params,
                           const PlanContext& ctx) {
  if (ctx.data_dir.empty()) return RpcError(-32000, "data_dir 未配置");
  if (!params.HasMember("name") || !params["name"].IsString()) {
    return RpcError(400, "name 必填");
  }
  std::lock_guard<std::mutex> lock(*ctx.file_mutex);
  for (const auto& profile : LoadProfiles(ProfilesPath(ctx))) {
    if (profile.name == params["name"].GetString()) {
      return {true, RenderProfileJson(profile), 0, ""};
    }
  }
  return RpcError(404, "profile 不存在");
}

RpcResult HandleProfileUpsert(const rapidjson::Value& params,
                              const PlanContext& ctx) {
  if (ctx.data_dir.empty()) return RpcError(-32000, "data_dir 未配置");
  if (!params.HasMember("profile") || !params["profile"].IsObject()) {
    return RpcError(400, "profile 必填");
  }
  Profile incoming;
  if (!ParseProfile(params["profile"], &incoming)) {
    return RpcError(400, "profile 形状非法");
  }
  if (const char* issue = ValidateProfile(incoming)) {
    return RpcError(400, "profile 校验失败: " + std::string(issue));
  }
  std::lock_guard<std::mutex> lock(*ctx.file_mutex);
  auto profiles = LoadProfiles(ProfilesPath(ctx));
  bool replaced = false;
  for (auto& profile : profiles) {
    if (profile.name == incoming.name) {
      profile = incoming;
      replaced = true;
      break;
    }
  }
  if (!replaced) profiles.push_back(incoming);
  auto stored = StoreProfiles(ProfilesPath(ctx), profiles);
  if (!stored.has_value()) {
    return RpcError(-32000, stored.error().message);
  }
  return {true, RenderProfileJson(incoming), 0, ""};
}

RpcResult HandleProfileDelete(const rapidjson::Value& params,
                              const PlanContext& ctx) {
  if (ctx.data_dir.empty()) return RpcError(-32000, "data_dir 未配置");
  if (!params.HasMember("name") || !params["name"].IsString()) {
    return RpcError(400, "name 必填");
  }
  std::lock_guard<std::mutex> lock(*ctx.file_mutex);
  auto profiles = LoadProfiles(ProfilesPath(ctx));
  bool removed = false;
  profiles.erase(
      std::remove_if(profiles.begin(), profiles.end(),
                     [&](const Profile& profile) {
                       if (profile.name == params["name"].GetString()) {
                         removed = true;
                         return true;
                       }
                       return false;
                     }),
      profiles.end());
  auto stored = StoreProfiles(ProfilesPath(ctx), profiles);
  if (!stored.has_value()) {
    return RpcError(-32000, stored.error().message);
  }
  if (!removed) return RpcError(404, "profile 不存在");
  // Writer 纪律（CLAUDE.md：禁止手写 JSON 字面量）。
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  writer.StartObject();
  writer.Key("ok");
  writer.Bool(true);
  writer.EndObject();
  return {true, buffer.GetString(), 0, ""};
}

// list_cycles（决策 18：px 包装；Phase 9 单周期）。
RpcResult HandleListCycles(const rapidjson::Value&, const PlanContext& ctx) {
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  writer.StartObject();
  writer.Key("cycles");
  writer.StartArray();
  if (ctx.cycle != 0) writer.Uint(ctx.cycle);
  writer.EndArray();
  writer.EndObject();
  return {true, buffer.GetString(), 0, ""};
}

// 透传端点统一错误（navdata 缺失时注册，决策 47）。
RpcResult NoDatabaseHandler(const rapidjson::Value&) { return NoDatabase(); }

}  // namespace

std::unordered_map<std::string, RpcHandler> MakePlanHandlers(PlanContext ctx) {
  // 决策 44：airframe 文件写串行化互斥（随 ctx 拷贝共享）。
  if (!ctx.file_mutex) ctx.file_mutex = std::make_shared<std::mutex>();
  std::unordered_map<std::string, RpcHandler> handlers;
  // 透传（决策 16：bf 8 个，find_routes 排除）；db 缺失时统一 -32000。
  if (ctx.db != nullptr) {
    auto bf_handlers = MakeBfHandlers(*ctx.db);
    for (auto& [name, handler] : bf_handlers) {
      handlers.emplace(name, std::move(handler));
    }
  } else {
    for (const auto& named : bf::service::MakeHandlers()) {
      if (named.name == "find_routes") continue;
      handlers.emplace(named.name, NoDatabaseHandler);
    }
  }
  // px 专属端点（决策 16 消息集）。
  handlers.emplace("plan.analyze", [ctx](const rapidjson::Value& params) {
    return HandleAnalyze(params, ctx);
  });
  handlers.emplace("plan.routes", [ctx](const rapidjson::Value& params) {
    return HandleRoutes(params, ctx);
  });
  handlers.emplace("plan.alternates", [ctx](const rapidjson::Value& params) {
    return HandleAlternates(params, ctx);
  });
  handlers.emplace("list_cycles", [ctx](const rapidjson::Value& params) {
    return HandleListCycles(params, ctx);
  });
  handlers.emplace("plan.generate", [ctx](const rapidjson::Value& params) {
    return HandleGenerate(params, ctx);
  });
  handlers.emplace("plan.export", [ctx](const rapidjson::Value& params) {
    return HandleExport(params, ctx);
  });
  handlers.emplace("airframe.list", [ctx](const rapidjson::Value& params) {
    return HandleAirframeList(params, ctx);
  });
  handlers.emplace("airframe.get", [ctx](const rapidjson::Value& params) {
    return HandleAirframeGet(params, ctx);
  });
  handlers.emplace("airframe.upsert", [ctx](const rapidjson::Value& params) {
    return HandleAirframeUpsert(params, ctx);
  });
  handlers.emplace("airframe.delete", [ctx](const rapidjson::Value& params) {
    return HandleAirframeDelete(params, ctx);
  });
  // profile 四端点（决策 55）。
  handlers.emplace("profile.list", [ctx](const rapidjson::Value& params) {
    return HandleProfileList(params, ctx);
  });
  handlers.emplace("profile.get", [ctx](const rapidjson::Value& params) {
    return HandleProfileGet(params, ctx);
  });
  handlers.emplace("profile.upsert", [ctx](const rapidjson::Value& params) {
    return HandleProfileUpsert(params, ctx);
  });
  handlers.emplace("profile.delete", [ctx](const rapidjson::Value& params) {
    return HandleProfileDelete(params, ctx);
  });
  return handlers;
}

}  // namespace px
