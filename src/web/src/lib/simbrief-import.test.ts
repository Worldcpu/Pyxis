// SimBrief 最小导入测试（决策 39：DOMParser 解析 OFP HTML——起降场/航路串/机型）。

import { describe, expect, it } from 'vitest';

import { parseSimBriefOFP } from './simbrief-import';

const SAMPLE_OFP = `
<!DOCTYPE html>
<html><head><title>Flight Plan</title></head><body>
  <div id="route_data">
    <input type="hidden" name="origin" value="ZUCK">
    <input type="hidden" name="destination" value="ZBAA">
    <input type="hidden" name="route" value="TONIN W80 MAKET">
    <input type="hidden" name="aircraft_type" value="A320">
    <input type="hidden" name="acf_variant" value="Fenix A320 CFM">
  </div>
</body></html>`;

describe('parseSimBriefOFP: input[name=...] 格式', () => {
  it('解析起降场/航路串/机型', () => {
    const result = parseSimBriefOFP(SAMPLE_OFP);
    expect(result).not.toBeNull();
    expect(result).toEqual({
      departure: 'ZUCK',
      arrival: 'ZBAA',
      routeString: 'TONIN W80 MAKET',
      airframeType: 'A320',
    });
  });

  it('非 HTML 输入返回 null（静默失败，按钮不炸）', () => {
    expect(parseSimBriefOFP('not html at all')).toBeNull();
    expect(parseSimBriefOFP('')).toBeNull();
  });

  it('缺 route 字段时仍返回起降场（最小容错）', () => {
    const html = `<input type="hidden" name="origin" value="KLAX">
                  <input type="hidden" name="destination" value="KJFK">`;
    const result = parseSimBriefOFP(html);
    expect(result).not.toBeNull();
    expect(result?.departure).toBe('KLAX');
    expect(result?.arrival).toBe('KJFK');
    expect(result?.routeString).toBe('');
  });
});

describe('parseSimBriefOFP: id=route_* 旧格式回退', () => {
  it('div id 格式也能解析', () => {
    const html = `<div id="route_origin">ZUUU</div>
                  <div id="route_destination">ZYTX</div>
                  <div id="route_route">BIDIB W32</div>`;
    const result = parseSimBriefOFP(html);
    expect(result).toEqual({
      departure: 'ZUUU',
      arrival: 'ZYTX',
      routeString: 'BIDIB W32',
      airframeType: '',
    });
  });
});
