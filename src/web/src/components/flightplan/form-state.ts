// 生成前表单状态（ui-spec §3 六分区；参数指纹用于候选过期检测 决策 25/26）。

export interface PlanFormState {
  callsign: string;
  departure: string;
  arrival: string;
  etd: string;
  airframeType: string;
  airframeVariant: string;
  payloadMode: 'pax' | 'zfw';
  paxCount: string;
  cargoKg: string;
  zfwKg: string;
  alternate: string;
  costIndex: string;
  routeString: string;
  // 高级（决策 25/27）：高度带 + 规则。
  minFl: string;
  maxFl: string;
  altitudeRule: 'auto' | 'icao' | 'china';
  // Fuel（决策 11/21；Phase 10 填充数值，表单先立住）。
  fuelPolicy: string;
  extraFuelKg: string;
  taxiMinutes: string;
}

export const INITIAL_FORM: PlanFormState = {
  callsign: '',
  departure: '',
  arrival: '',
  etd: '',
  airframeType: '',
  airframeVariant: '',
  payloadMode: 'pax',
  paxCount: '0',
  cargoKg: '0',
  zfwKg: '',
  alternate: '',
  costIndex: '0',
  routeString: '',
  minFl: '250',
  maxFl: '410',
  altitudeRule: 'auto',
  fuelPolicy: '0',
  extraFuelKg: '',
  taxiMinutes: '15',
};

/**
 * 候选参数指纹：影响候选结果的字段序列化。
 * 变化后旧候选视为过期（ui-spec §6 警告条橙黄）。
 * 不含 routeString——它是候选点选的产物而非 plan.routes 参数，
 * 纳入会导致正常点选自触发"候选过期"（审查修复）。
 */
export function candidatesFingerprint(f: PlanFormState): string {
  return JSON.stringify({
    d: f.departure.trim().toUpperCase(),
    a: f.arrival.trim().toUpperCase(),
    min: f.minFl,
    max: f.maxFl,
    rule: f.altitudeRule,
  });
}
