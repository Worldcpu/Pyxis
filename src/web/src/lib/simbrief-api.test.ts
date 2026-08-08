// SimBrief XML 拉取分类测试（S9.1 D52：四分类失败 + 成功解析）。

import { afterEach, describe, expect, it, vi } from 'vitest';

import { fetchSimBriefOFP, parseSimBriefXml } from './simbrief-api';

const OFP_XML = `<?xml version="1.0"?>
<OFP>
  <origin><icao_code>ZUCK</icao_code></origin>
  <destination><icao_code>ZBAA</icao_code></destination>
  <general><route>ZUCK TONIN W80 MAKET ZBAA</route></general>
  <aircraft><icao_code>A320</icao_code></aircraft>
</OFP>`;

afterEach(() => {
  vi.unstubAllGlobals();
});

describe('parseSimBriefXml', () => {
  it('成功解析：起降场/航路串/机型', () => {
    const r = parseSimBriefXml(OFP_XML);
    expect(r.ok).toBe(true);
    if (r.ok) {
      expect(r.data).toEqual({
        departure: 'ZUCK',
        arrival: 'ZBAA',
        routeString: 'ZUCK TONIN W80 MAKET ZBAA',
        airframeType: 'A320',
      });
    }
  });

  it('非 OFP XML → parse', () => {
    const r = parseSimBriefXml('<html><body>hello</body></html>');
    expect(r.ok).toBe(false);
    if (!r.ok) expect(r.failure.kind).toBe('parse');
  });

  it('200 但缺航路 → no-flight-plan', () => {
    const r = parseSimBriefXml(
      '<OFP><origin><icao_code>ZUCK</icao_code></origin></OFP>',
    );
    expect(r.ok).toBe(false);
    if (!r.ok) expect(r.failure.kind).toBe('no-flight-plan');
  });

  it('缺起降场 → parse', () => {
    const r = parseSimBriefXml(
      '<OFP><general><route>A B C</route></general></OFP>',
    );
    expect(r.ok).toBe(false);
    if (!r.ok) expect(r.failure.kind).toBe('parse');
  });
});

describe('fetchSimBriefOFP', () => {
  it('空/非数字 pilotid → invalid-pilotid（不发请求）', async () => {
    const fetchSpy = vi.fn();
    vi.stubGlobal('fetch', fetchSpy);
    const r = await fetchSimBriefOFP('');
    expect(r.ok).toBe(false);
    if (!r.ok) expect(r.failure.kind).toBe('invalid-pilotid');
    expect(fetchSpy).not.toHaveBeenCalled();
  });

  it('fetch 异常 → network', async () => {
    vi.stubGlobal('fetch', vi.fn(async () => {
      throw new TypeError('Failed to fetch');
    }));
    const r = await fetchSimBriefOFP('123456');
    expect(r.ok).toBe(false);
    if (!r.ok) expect(r.failure.kind).toBe('network');
  });

  it('404 → invalid-pilotid（SimBrief 未知 userid 行为）', async () => {
    vi.stubGlobal(
      'fetch',
      vi.fn(async () => new Response('Not Found', { status: 404 })),
    );
    const r = await fetchSimBriefOFP('999999');
    expect(r.ok).toBe(false);
    if (!r.ok) expect(r.failure.kind).toBe('invalid-pilotid');
  });

  it('200 + 合法 OFP → 成功数据', async () => {
    vi.stubGlobal(
      'fetch',
      vi.fn(async () =>
        new Response(OFP_XML, {
          status: 200,
          headers: { 'Content-Type': 'text/xml' },
        }),
      ),
    );
    const r = await fetchSimBriefOFP('123456');
    expect(r.ok).toBe(true);
    if (r.ok) expect(r.data.departure).toBe('ZUCK');
  });
});
