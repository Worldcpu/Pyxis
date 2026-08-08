// SimBrief OFP XML 拉取（S9.1 D52：pilotid → xml.fetcher API → 分类失败）。
// 失败四分类（epic T5）：invalid-pilotid / no-flight-plan / network / parse；
// 404 = 未知 userid（SimBrief 行为）→ invalid-pilotid；200 但缺航路 →
// no-flight-plan；XML 缺关键字段 → parse。CORS 受限属 best-effort（Phase 10
// 正式凭据管理）。

import type { SimBriefImport } from './simbrief-import';

export type SimBriefFailure =
  | { kind: 'invalid-pilotid' }
  | { kind: 'no-flight-plan' }
  | { kind: 'network' }
  | { kind: 'parse' };

export type SimBriefFetchResult =
  | { ok: true; data: SimBriefImport }
  | { ok: false; failure: SimBriefFailure };

/** SimBrief OFP API（pilotid 最佳努力；无凭据）。 */
export const SIMBRIEF_FETCHER_URL =
  'https://www.simbrief.com/api/xml.fetcher.php';

/** 首个元素的子标签文本（空则 ''）。 */
function textOf(parent: Element | null, tag: string): string {
  const el = parent?.getElementsByTagName(tag)[0];
  return el?.textContent?.trim() ?? '';
}

/** 解析 OFP XML（best-effort 字段路径）；缺航路 → no-flight-plan，其余 → parse。 */
export function parseSimBriefXml(xml: string): SimBriefFetchResult {
  let doc: Document;
  try {
    doc = new DOMParser().parseFromString(xml, 'text/xml');
  } catch {
    return { ok: false, failure: { kind: 'parse' } };
  }
  if (!doc.querySelector('OFP') && !doc.querySelector('ofp')) {
    return { ok: false, failure: { kind: 'parse' } };
  }
  const origin = doc.getElementsByTagName('origin')[0];
  const destination = doc.getElementsByTagName('destination')[0];
  const general = doc.getElementsByTagName('general')[0];
  const aircraft = doc.getElementsByTagName('aircraft')[0];
  const departure = textOf(origin, 'icao_code').toUpperCase();
  const arrival = textOf(destination, 'icao_code').toUpperCase();
  const routeString = textOf(general, 'route').toUpperCase();
  const airframeType = textOf(aircraft, 'icao_code').toUpperCase();
  if (!routeString) {
    return { ok: false, failure: { kind: 'no-flight-plan' } };
  }
  if (!departure || !arrival) {
    return { ok: false, failure: { kind: 'parse' } };
  }
  return {
    ok: true,
    data: { departure, arrival, routeString, airframeType },
  };
}

/**
 * 按 pilotid 拉取 OFP。
 * 分类：空/非数字 pilotid → invalid-pilotid；fetch 异常 → network；
 * HTTP 404/非 200 → invalid-pilotid；200 → parseSimBriefXml 分类。
 */
export async function fetchSimBriefOFP(
  pilotid: string,
): Promise<SimBriefFetchResult> {
  const id = pilotid.trim();
  if (!id || !/^\d+$/.test(id)) {
    return { ok: false, failure: { kind: 'invalid-pilotid' } };
  }
  let res: Response;
  try {
    res = await fetch(`${SIMBRIEF_FETCHER_URL}?userid=${encodeURIComponent(id)}`);
  } catch {
    return { ok: false, failure: { kind: 'network' } };
  }
  if (!res.ok) {
    return { ok: false, failure: { kind: 'invalid-pilotid' } };
  }
  const xml = await res.text().catch(() => '');
  return parseSimBriefXml(xml);
}
