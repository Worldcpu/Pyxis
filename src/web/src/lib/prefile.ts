// Prefile URL 构建（决策 38：VATSIM raw FPL / IVAO base64 JSON /
// PilotEdge 平铺 query）。巡航速度取自 airframe 档案（决策 38 契约）。

export interface PrefileInput {
  callsign: string;
  departure: string;
  arrival: string;
  /** ETD，HHMM 格式（如 1040）。 */
  etd: string;
  cruiseFl: number;
  /** 巡航速度 kt（airframe 档案字段；缺省则调用方提示不生成）。 */
  cruiseSpeedKt: number;
  airframeType: string;
  routeString: string;
  alternate?: string;
}

/** ICAO FPL 编码（最小集；设备码/导航能力为固定简化值）。 */
export function buildIcaoFpl(input: PrefileInput): string {
  const speed = String(input.cruiseSpeedKt).padStart(4, '0');
  const lines = [
    `(FPL-${input.callsign || 'PYXIS'}-IG`,
    `-${input.airframeType || 'ZZZZ'}/M-SDE2E3FGHIRWY/S`,
    `-${input.departure}${input.etd}`,
    `-N${speed}F${input.cruiseFl} ${input.routeString}`,
    `-${input.arrival}${input.alternate ? ` ${input.alternate}` : ''}`,
    `-RMK/PYXIS)`,
  ];
  return lines.join('\n');
}

function fuelTime(etd: string): string {
  // fuel_time 取 HHMM 推后约 3 小时（粗糙默认；Phase 10 燃油引擎后精确）。
  const hh = Number(etd.slice(0, 2) || 0);
  const mm = Number(etd.slice(2, 4) || 0);
  const total = (hh * 60 + mm + 180) % 1440;
  return `${String(Math.floor(total / 60)).padStart(2, '0')}${String(total % 60).padStart(2, '0')}`;
}

/** VATSIM prefile URL（raw FPL + fuel_time）。 */
export function buildVatsimUrl(input: PrefileInput): string {
  const q = new URLSearchParams({
    raw: buildIcaoFpl(input),
    fuel_time: fuelTime(input.etd),
  });
  return `https://www.vatsim.net/flightplan?${q.toString()}`;
}

/** UTF-8 安全 base64（btoa 遇非 Latin-1 抛异常——审查修复）。 */
function utf8ToBase64(s: string): string {
  return btoa(String.fromCharCode(...new TextEncoder().encode(s)));
}

/** IVAO prefile URL（base64 JSON）。 */
export function buildIvaoUrl(input: PrefileInput): string {
  const payload = {
    callsign: input.callsign,
    aircraft: input.airframeType,
    departure: input.departure,
    destination: input.arrival,
    cruisingLevel: `F${input.cruiseFl}`,
    cruisingSpeed: input.cruiseSpeedKt,
    route: input.routeString,
    alternate: input.alternate ?? '',
  };
  const q = new URLSearchParams({ flightPlan: utf8ToBase64(JSON.stringify(payload)) });
  return `https://www.ivao.aero/flightplan?${q.toString()}`;
}

/** PilotEdge prefile URL（平铺 flightplan[...] query）。 */
export function buildPilotEdgeUrl(input: PrefileInput): string {
  const q = new URLSearchParams({
    'flightplan[type]': 'IFR',
    'flightplan[callsign]': input.callsign,
    'flightplan[departure_icao]': input.departure,
    'flightplan[arrival_icao]': input.arrival,
    'flightplan[cruising_speed]': String(input.cruiseSpeedKt),
    'flightplan[cruising_altitude]': String(input.cruiseFl * 100),
    'flightplan[route]': input.routeString,
    ...(input.alternate ? { 'flightplan[alternate]': input.alternate } : {}),
  });
  return `https://www.pilotedge.net/flight_plans/new?${q.toString()}`;
}
