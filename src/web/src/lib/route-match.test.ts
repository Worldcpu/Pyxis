// isSameRoute 纯函数测试（S9.1 D55 rev.：归一化匹配）。

import { describe, expect, it } from 'vitest';

import { isSameRoute, normalizeRouteString } from './route-match';

describe('normalizeRouteString', () => {
  it('折叠连续空白 + 大写 + 去首尾', () => {
    expect(normalizeRouteString('  zuck   tonin  W80 maket  zbAA ')).toBe(
      'ZUCK TONIN W80 MAKET ZBAA',
    );
  });
});

describe('isSameRoute', () => {
  it('大小写/空格差异视为同一航路', () => {
    expect(isSameRoute('zuck tonin w80 zbAA', 'ZUCK  TONIN  W80  ZBAA')).toBe(
      true,
    );
  });

  it('不同航路 → false', () => {
    expect(isSameRoute('ZUCK TONIN W80 ZBAA', 'ZUCK TONIN W81 ZBAA')).toBe(
      false,
    );
  });

  it('空白串边界：空 vs 空 → true', () => {
    expect(isSameRoute('', '   ')).toBe(true);
  });
});
