// Vitest 全局 setup：jest-dom 匹配器 + RTL cleanup（globals=false 时需显式注册）
// + Radix 组件所需的 jsdom pointer capture polyfill。

import '@testing-library/jest-dom/vitest';
import { cleanup } from '@testing-library/react';
import { afterEach } from 'vitest';

// jsdom 未实现 pointer capture / scrollIntoView（Radix Select 等依赖）。
if (!Element.prototype.hasPointerCapture) {
  Element.prototype.hasPointerCapture = () => false;
  Element.prototype.setPointerCapture = () => {};
  Element.prototype.releasePointerCapture = () => {};
}
if (!Element.prototype.scrollIntoView) {
  Element.prototype.scrollIntoView = () => {};
}
// matchMedia（审查修复：ThemeProvider/MapView 渲染期调用，jsdom 未实现）。
if (!window.matchMedia) {
  window.matchMedia = (query: string) => ({
    matches: false,
    media: query,
    onchange: null,
    addEventListener: () => {},
    removeEventListener: () => {},
    addListener: () => {},
    removeListener: () => {},
    dispatchEvent: () => false,
  });
}

afterEach(() => cleanup());
