// 单位换算（决策 24：协议固定 SI 基准，前端仅做显示换算）。
// 换算系数：1 FL = 100 ft = 30.48 m；1 NM = 1852 m。

/** 飞行高度层 → 米（FL350 = 10668 m）。 */
export function flToMeters(fl: number): number {
  return fl * 30.48;
}

/** 米 → 飞行高度层（取整到最近 FL）。 */
export function metersToFl(m: number): number {
  return Math.round(m / 30.48);
}

/** 海里 → 公里。 */
export function nmToKm(nm: number): number {
  return nm * 1.852;
}

/** 格式化为 FLxxx。 */
export function formatFl(fl: number): string {
  return `FL${fl}`;
}

/** 双单位显示：巡航高度 FLxxx / xxx m。 */
export function formatAlt(fl: number): string {
  return `${formatFl(fl)} / ${Math.round(flToMeters(fl))} m`;
}
