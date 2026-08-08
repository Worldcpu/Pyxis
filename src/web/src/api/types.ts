// px_server JSON-RPC 端点类型（对齐 plan_handlers.cc / plan_json.cc 契约）。
// 宽松字段（fuel 等 Phase 10 占位）标 optional。
// 契约漂移修复（code review）：候选形状顶层 points / via+segment_index /
// total_distance_nm；checks.status 字符串；alternates 带 lat/lon。

/** 航路点（服务器写 ident/via/lat/lon/segment_index；-1 = 机场点）。 */
export interface RoutePoint {
  ident: string;
  /** 所属航路（via）。 */
  via?: string;
  lat: number;
  lon: number;
  segment_index: number;
}

export type CheckStatus = 'ok' | 'warning' | 'unflyable';

/** plan.generate 响应。 */
export interface FlightPlan {
  route: {
    points: RoutePoint[];
    /** 完整航路串（审查修复：Prefile/显示用权威值，候选回填可能为空）。 */
    route_string?: string;
    sid?: string;
    star?: string;
  };
  altitude: {
    fl: number;
    meters: number;
    /** true = 手动输入，false = 自动（候选/无风降级）。 */
    manual: boolean;
    rationale: string;
  };
  // 五字段回显（决策 45）+ mora_checked（决策 26 修订）。
  callsign?: string;
  etd?: string;
  alternate?: string;
  seed?: number;
  mora_checked?: boolean;
  checks?: { status: CheckStatus; warnings: string[] };
  /** weights 仅 dow/zfw/tow/lw 现网输出（其余 Phase 10）——全可选。 */
  weights?: {
    dow_kg?: number;
    zfw_kg?: number;
    tow_kg?: number;
    lw_kg?: number;
    mzfw_kg?: number;
    mtow_kg?: number;
    mlw_kg?: number;
  };
  /** 燃油（Phase 10 填充）。 */
  fuel?: unknown;
  distances?: { dep_nm?: number; enroute_nm?: number; arr_nm?: number };
  distance_nm?: number;
}

/** plan.routes 候选（RenderPlanCandidatesJson：points 顶层，无 route 对象）。 */
export interface RouteCandidate {
  index: number;
  route_string: string;
  total_distance_nm?: number;
  distances?: { dep_nm?: number; enroute_nm?: number; arr_nm?: number };
  sid?: string;
  star?: string;
  dep_runway?: string;
  arr_runway?: string;
  dep_connection?: string;
  arr_connection?: string;
  seed?: number;
  points: RoutePoint[];
}

/** plan.alternates 条目（决策 12 修订：{icao, distance_nm, lat, lon}）。 */
export interface Alternate {
  icao: string;
  distance_nm: number;
  lat: number;
  lon: number;
  route?: string;
}

export type PerfSource = 'lnm' | 'custom' | 'openap' | 'fcom';

export interface Airframe {
  type: string;
  variant: string;
  perf_source: PerfSource;
  dow_kg: number;
  mzfw_kg: number;
  mtow_kg: number;
  mlw_kg: number;
  service_ceiling_ft: number;
  unit_pax_kg: number;
  unit_bag_kg: number;
  /** 巡航速度（决策 38，可选 >0；供 Prefile FPL 编码）。 */
  cruise_speed_kt?: number;
}

export interface PlanExportResult {
  format: string;
  filename: string;
  content: string;
}

export interface ListCyclesResult {
  cycles: number[];
}

/** 候选搜索参数（plan.routes；决策 7/9）。 */
export interface RoutesParams {
  departure: string;
  arrival: string;
  k?: number;
  min_fl?: number;
  max_fl?: number;
  level?: 'low' | 'high';
  departure_runway?: string;
  arrival_runway?: string;
  departure_sid?: string;
  arrival_star?: string;
  avoid_waypoints?: string[];
  forced_points?: string[];
  random_seed?: number;
}

/** 计划生成参数（plan.generate；决策 8/13/14/23/26/27/45）。 */
export interface GenerateParams {
  route_string: string;
  airframe: Airframe;
  callsign?: string;
  etd?: string;
  alternate?: string;
  cruise_fl?: number;
  altitude_rule?: 'auto' | 'icao' | 'china';
  min_fl?: number;
  max_fl?: number;
  pax_count?: number;
  cargo_kg?: number;
  zfw_kg?: number;
  /** 候选标记（决策 26 修订：候选搜索已带 MORA）。 */
  candidate?: boolean;
  random_seed?: number;
}

/** 备降搜索参数（plan.alternates；决策 12/48）。 */
export interface AlternatesParams {
  arrival: string;
  max_distance_nm?: number;
  avoid_icaos?: string[];
}

/** plan.analyze 响应（决策 54 + S9.1 补丁：valid 时带完整点序列）。 */
export interface AnalyzeResult {
  valid: boolean;
  cycle: number;
  distance_nm?: number;
  errors?: { message: string }[];
  /** valid=true 时：解析航路的完整点序列（规划航路图层）。 */
  points?: RoutePoint[];
}

/** 偏好档案（决策 55：plan.routes 参数子集，服务端 profiles.json 持久化）。 */
export interface Profile {
  name: string;
  k?: number;
  level?: 'low' | 'high';
  min_fl?: number;
  max_fl?: number;
  avoid_waypoints?: string[];
}
