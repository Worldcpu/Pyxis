// 应用壳布局回归测试：桌面导航栏贴左占满整高的玻璃 rail（1:1 常驻后
// 飞行计划图标为纯导航，无抽屉）。

import { render } from '@testing-library/react';
import i18n from 'i18next';
import { beforeAll, describe, expect, it } from 'vitest';
import { MemoryRouter } from 'react-router-dom';

import { AppShell } from './app-shell';
import { ThemeProvider } from './theme-provider';
import { TooltipProvider } from './ui/tooltip';
import '../i18n';

beforeAll(async () => {
  await i18n.init();
});

describe('AppShell: 桌面导航栏', () => {
  it('贴左占满整高：无左侧/上下间隙，右缘 border-r 为面板分割线', () => {
    render(
      <TooltipProvider>
        <ThemeProvider defaultTheme="light">
          <MemoryRouter>
            <AppShell />
          </MemoryRouter>
        </ThemeProvider>
      </TooltipProvider>,
    );
    const rail = document.querySelector('aside') as HTMLElement;
    expect(rail).not.toBeNull();
    expect(rail.className).toContain('absolute');
    expect(rail.className).toContain('inset-y-0');
    expect(rail.className).toContain('left-0');
    expect(rail.className).toContain('border-r');
    expect(rail.className).toContain('z-30');
  });
});
