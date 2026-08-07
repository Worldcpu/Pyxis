// Prefile URL 构建测试（决策 38：VATSIM raw FPL / IVAO base64 JSON /
// PilotEdge 平铺 query；巡航速度取自 airframe 档案）。

import { describe, expect, it } from 'vitest';

import {
  buildVatsimUrl,
  buildIvaoUrl,
  buildPilotEdgeUrl,
  buildIcaoFpl,
  type PrefileInput,
} from './prefile';

const INPUT: PrefileInput = {
  callsign: 'CCA4101',
  departure: 'ZUCK',
  arrival: 'ZBAA',
  etd: '1040',
  cruiseFl: 350,
  cruiseSpeedKt: 437,
  airframeType: 'A320',
  routeString: 'TONIN W80 MAKET',
  alternate: 'ZLXY',
};

describe('ICAO FPL 编码', () => {
  it('含呼号/机型/速度/高度/起降场/ETD/航路串/备降', () => {
    const fpl = buildIcaoFpl(INPUT);
    expect(fpl).toContain('FPL-CCA4101-IG');
    expect(fpl).toContain('A320/M');
    expect(fpl).toContain('N0437F350'); // 巡航速度 437kt + FL350
    expect(fpl).toContain('ZUCK1040');
    expect(fpl).toContain('TONIN W80 MAKET');
    expect(fpl).toContain('ZBAA');
    expect(fpl).toContain('ZLXY'); // 备降
  });
});

describe('VATSIM URL', () => {
  it('flightplan?raw= 编码 FPL + fuel_time', () => {
    const url = buildVatsimUrl(INPUT);
    expect(url).toMatch(/^https:\/\/www\.vatsim\.net\/flightplan\?/);
    const q = new URLSearchParams(url.split('?')[1]);
    expect(q.get('raw')).toContain('FPL-CCA4101-IG');
    expect(q.get('fuel_time')).not.toBeNull();
  });
});

describe('IVAO URL', () => {
  it('flightPlan= base64 JSON（含起降场/航路/高度）', () => {
    const url = buildIvaoUrl(INPUT);
    expect(url).toMatch(/^https:\/\/www\.ivao\.aero\/flightplan\?/);
    const q = new URLSearchParams(url.split('?')[1]);
    const json = JSON.parse(atob(q.get('flightPlan') ?? ''));
    expect(json.callsign).toBe('CCA4101');
    expect(json.departure).toBe('ZUCK');
    expect(json.destination).toBe('ZBAA');
    expect(json.route).toContain('TONIN');
    expect(json.cruisingSpeed).toBe(437);
    expect(json.cruisingLevel).toBe('F350');
  });
});

describe('PilotEdge URL', () => {
  it('平铺 flightplan[...] query 字段', () => {
    const url = buildPilotEdgeUrl(INPUT);
    expect(url).toMatch(/^https:\/\/www\.pilotedge\.net\/flight_plans\/new\?/);
    const q = new URLSearchParams(url.split('?')[1]);
    expect(q.get('flightplan[type]')).toBe('IFR');
    expect(q.get('flightplan[departure_icao]')).toBe('ZUCK');
    expect(q.get('flightplan[arrival_icao]')).toBe('ZBAA');
    expect(q.get('flightplan[cruising_speed]')).toBe('437');
    expect(q.get('flightplan[cruising_altitude]')).toBe('35000');
    expect(q.get('flightplan[route]')).toBe('TONIN W80 MAKET');
    expect(q.get('flightplan[alternate]')).toBe('ZLXY');
  });
});
