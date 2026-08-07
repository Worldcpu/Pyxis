// 主题三态 provider（决策 33：亮/暗/跟随，默认跟随系统，localStorage 持久化）。
// 持久化 key 'px-theme'；显式亮/暗写 class，system 跟随 matchMedia 变化。

import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useLayoutEffect,
  useMemo,
  useState,
  type ReactNode,
} from 'react';

export type Theme = 'light' | 'dark' | 'system';

interface ThemeContextValue {
  theme: Theme;
  /** 实际生效主题（system 已解析——审查修复：地图底图等消费方此前
   *  自算 matchMedia，system 下 OS 切换不跟随）。 */
  resolvedTheme: 'light' | 'dark';
  setTheme: (theme: Theme) => void;
}

const ThemeContext = createContext<ThemeContextValue | undefined>(undefined);

const STORAGE_KEY = 'px-theme';

function systemPrefersDark(): boolean {
  return window.matchMedia('(prefers-color-scheme: dark)').matches;
}

function applyThemeClass(theme: Theme): void {
  const dark = theme === 'dark' || (theme === 'system' && systemPrefersDark());
  document.documentElement.classList.toggle('dark', dark);
}

export function ThemeProvider({
  defaultTheme = 'system',
  children,
}: {
  defaultTheme?: Theme;
  children: ReactNode;
}) {
  const [theme, setThemeState] = useState<Theme>(() => {
    const stored = localStorage.getItem(STORAGE_KEY);
    return stored === 'light' || stored === 'dark' || stored === 'system'
      ? stored
      : defaultTheme;
  });

  // resolved 状态跟随 theme + 系统变化（审查修复：此前 system 切换
  // 只改 class 无 state——MapView 等消费方不重渲染）。
  const [resolved, setResolved] = useState<'light' | 'dark'>(() =>
    theme === 'system'
      ? systemPrefersDark()
        ? 'dark'
        : 'light'
      : theme,
  );
  useEffect(() => {
    const update = () =>
      setResolved(
        theme === 'system'
          ? systemPrefersDark()
            ? 'dark'
            : 'light'
          : theme,
      );
    update();
    if (theme !== 'system') return;
    const media = window.matchMedia('(prefers-color-scheme: dark)');
    media.addEventListener('change', update);
    return () => media.removeEventListener('change', update);
  }, [theme]);

  // 首帧即应用主题 class（useLayoutEffect——审查修复：useEffect 在
  // paint 后执行，存储暗色时先亮后暗闪烁）。
  useLayoutEffect(() => {
    applyThemeClass(resolved);
  }, [resolved]);

  const setTheme = useCallback((next: Theme) => {
    setThemeState(next);
    if (next === 'system') {
      localStorage.removeItem(STORAGE_KEY);
    } else {
      localStorage.setItem(STORAGE_KEY, next);
    }
  }, []);

  const value = useMemo(
    () => ({ theme, resolvedTheme: resolved, setTheme }),
    [theme, resolved, setTheme],
  );
  return <ThemeContext.Provider value={value}>{children}</ThemeContext.Provider>;
}

export function useTheme(): ThemeContextValue {
  const ctx = useContext(ThemeContext);
  if (!ctx) throw new Error('useTheme 必须在 ThemeProvider 内使用');
  return ctx;
}
