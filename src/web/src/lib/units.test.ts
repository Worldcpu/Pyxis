// 单位换算测试（决策 24：协议固定 SI 基准——kg/FL/NM/kt）。
// 换算系数独立真源：1 ft = 0.3048 m（精确）；1 NM = 1852 m（精确）。

import { describe, expect, it } from 'vitest';

import { flToMeters, metersToFl, nmToKm, formatAlt, formatFl } from './units';

describe('flight level 换算（FL = 百英尺，30.48 m/FL）', () => {
  it('flToMeters: FL350 = 10668 m（350 × 100 × 0.3048）', () => {
    expect(flToMeters(350)).toBeCloseTo(10668, 6);
  });

  it('flToMeters: FL0 与负值边界', () => {
    expect(flToMeters(0)).toBe(0);
    expect(flToMeters(-10)).toBeCloseTo(-304.8, 6);
  });

  it('metersToFl: 往返一致（10668 m → FL350）', () => {
    expect(metersToFl(10668)).toBe(350);
  });

  it('metersToFl: 取整到最近 FL', () => {
    expect(metersToFl(10000)).toBe(328); // 10000/30.48 ≈ 328.08
  });
});

describe('距离换算', () => {
  it('nmToKm: 1 NM = 1.852 km', () => {
    expect(nmToKm(1)).toBeCloseTo(1.852, 6);
    expect(nmToKm(850)).toBeCloseTo(1574.2, 4);
  });
});

describe('显示格式化', () => {
  it('formatFl: FL226', () => {
    expect(formatFl(226)).toBe('FL226');
  });

  it('formatAlt: 双单位 FLxxx / xxx m', () => {
    expect(formatAlt(350)).toBe('FL350 / 10668 m');
  });
});
