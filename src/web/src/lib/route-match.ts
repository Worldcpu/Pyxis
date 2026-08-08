// 航路串归一化比较（S9.1 D55 rev.：候选卡片与 Route 输入的匹配判定）。
// 归一化：折叠空白 + 大写——候选回填/手写输入的空格差异视为同一航路。

/** 归一化航路串：大写 + 连续空白折叠为单个空格 + 去首尾空白。 */
export function normalizeRouteString(s: string): string {
  return s.trim().toUpperCase().replace(/\s+/g, ' ');
}

/** 两航路串归一化后相等（"Route input route" 徽章判定）。 */
export function isSameRoute(a: string, b: string): boolean {
  return normalizeRouteString(a) === normalizeRouteString(b);
}
