// 主题三态 provider 测试（决策 33：亮/暗/跟随三态，默认跟随，localStorage 持久化）。

import { act, render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import { ThemeProvider, useTheme } from './theme-provider';

function TestConsumer() {
  const { theme, setTheme } = useTheme();
  return (
    <div>
      <span data-testid="theme">{theme}</span>
      <button onClick={() => setTheme('dark')}>dark</button>
      <button onClick={() => setTheme('light')}>light</button>
      <button onClick={() => setTheme('system')}>system</button>
    </div>
  );
}

function setup(initial?: 'light' | 'dark' | 'system') {
  return render(
    <ThemeProvider defaultTheme={initial}>{<TestConsumer />}</ThemeProvider>,
  );
}

beforeEach(() => {
  localStorage.clear();
  document.documentElement.className = '';
});

afterEach(() => {
  vi.unstubAllGlobals();
});

describe('theme-provider: 默认与持久化', () => {
  it('默认跟随系统（无显式 theme 时读 prefers-color-scheme）', () => {
    vi.stubGlobal('matchMedia', () => ({
      matches: false,
      addEventListener: vi.fn(),
      removeEventListener: vi.fn(),
    }));
    setup();
    expect(screen.getByTestId('theme').textContent).toBe('system');
    // 系统为亮色 → 无 dark class
    expect(document.documentElement.classList.contains('dark')).toBe(false);
  });

  it('设置 dark → documentElement 加 dark class + localStorage 持久化', async () => {
    vi.stubGlobal('matchMedia', () => ({
      matches: false,
      addEventListener: vi.fn(),
      removeEventListener: vi.fn(),
    }));
    setup();
    await act(async () => {
      await userEvent.click(screen.getByText('dark'));
    });
    expect(document.documentElement.classList.contains('dark')).toBe(true);
    expect(localStorage.getItem('px-theme')).toBe('dark');
  });

  it('重渲染时从 localStorage 恢复 dark', () => {
    localStorage.setItem('px-theme', 'dark');
    setup();
    expect(screen.getByTestId('theme').textContent).toBe('dark');
    expect(document.documentElement.classList.contains('dark')).toBe(true);
  });

  it('切回 system → 移除 dark class 且 localStorage 清除', async () => {
    localStorage.setItem('px-theme', 'dark');
    vi.stubGlobal('matchMedia', () => ({
      matches: false,
      addEventListener: vi.fn(),
      removeEventListener: vi.fn(),
    }));
    setup();
    await act(async () => {
      await userEvent.click(screen.getByText('system'));
    });
    expect(screen.getByTestId('theme').textContent).toBe('system');
    expect(document.documentElement.classList.contains('dark')).toBe(false);
    expect(localStorage.getItem('px-theme')).toBeNull();
  });

  it('system + 系统为暗色 → 加 dark class', () => {
    vi.stubGlobal('matchMedia', () => ({
      matches: true,
      addEventListener: vi.fn(),
      removeEventListener: vi.fn(),
    }));
    setup();
    expect(document.documentElement.classList.contains('dark')).toBe(true);
  });
});
