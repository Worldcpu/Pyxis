// 应用壳（决策 32）：垂直图标栏 + 模块路由；左下角主题三态切换。
// 响应式（决策 32）：lg(1024px) 以下降级为横菜单栏 + 顶部横幅。

import {
  Map,
  MonitorCog,
  Moon,
  Plane,
  Settings,
  Sun,
  TriangleAlert,
} from 'lucide-react';
import type { ReactNode } from 'react';
import { NavLink, Outlet } from 'react-router-dom';
import { useTranslation } from 'react-i18next';

import { useTheme, type Theme } from './theme-provider';
import { Button } from './ui/button';
import { Tooltip, TooltipContent, TooltipTrigger } from './ui/tooltip';
import { cn } from '../lib/utils';

const THEME_CYCLE: Theme[] = ['system', 'light', 'dark'];

function ThemeToggle({ compact = false }: { compact?: boolean }) {
  const { theme, setTheme } = useTheme();
  const { t } = useTranslation();
  const next = THEME_CYCLE[(THEME_CYCLE.indexOf(theme) + 1) % THEME_CYCLE.length];
  const Icon = theme === 'light' ? Sun : theme === 'dark' ? Moon : MonitorCog;
  const label = t(`theme.${theme}`);
  return (
    <Tooltip>
      <TooltipTrigger asChild>
        <Button
          variant="ghost"
          size="icon"
          aria-label={`${label}（点击切换至 ${t(`theme.${next}`)}）`}
          onClick={() => setTheme(next)}
          className={compact ? 'h-8 w-8' : 'h-9 w-9'}
        >
          <Icon className="h-4 w-4" aria-hidden="true" />
        </Button>
      </TooltipTrigger>
      <TooltipContent>{label}</TooltipContent>
    </Tooltip>
  );
}

/** 模块图标按钮（决策 32：当前模块高亮指示 G6；占位模块灰显 + 提示）。 */
function ModuleButton({
  to,
  icon,
  label,
  disabled = false,
}: {
  to: string;
  icon: ReactNode;
  label: string;
  disabled?: boolean;
}) {
  const { t } = useTranslation();
  if (disabled) {
    return (
      <Tooltip>
        <TooltipTrigger asChild>
          <Button
            variant="ghost"
            size="icon"
            aria-label={label}
            className="h-9 w-9 opacity-40"
          >
            {icon}
          </Button>
        </TooltipTrigger>
        <TooltipContent>{`${label}（${t('nav.comingSoon')}）`}</TooltipContent>
      </Tooltip>
    );
  }
  return (
    <Tooltip>
      <TooltipTrigger asChild>
        <NavLink
          to={to}
          aria-label={label}
          className={({ isActive }) =>
            cn(
              'relative flex h-9 w-9 items-center justify-center rounded-md text-muted-foreground transition-colors hover:bg-muted hover:text-foreground',
              isActive &&
                'bg-muted text-foreground before:absolute before:left-0 before:top-1/2 before:h-4 before:w-0.5 before:-translate-y-1/2 before:rounded-full before:bg-secondary',
            )
          }
        >
          {icon}
        </NavLink>
      </TooltipTrigger>
      <TooltipContent>{label}</TooltipContent>
    </Tooltip>
  );
}

/** 响应式降级横幅（决策 32：窄窗口顶部提醒）。 */
function CompactBanner() {
  const { t } = useTranslation();
  return (
    <div
      role="status"
      className="flex items-center gap-1.5 border-b border-border bg-warning/10 px-3 py-1 text-xs text-warning lg:hidden"
    >
      <TriangleAlert className="h-3.5 w-3.5" aria-hidden="true" />
      {t('shell.compactHint')}
    </div>
  );
}

export function AppShell() {
  const { t } = useTranslation();
  return (
    <div className="flex h-screen w-full flex-col overflow-hidden">
      {/* 横菜单栏（<lg 降级布局） */}
      <div className="flex items-center gap-1 border-b border-border bg-card px-2 py-1 lg:hidden">
        <ModuleButton
          to="/"
          icon={<Plane className="h-4 w-4" aria-hidden="true" />}
          label={t('nav.flightplan')}
        />
        <ModuleButton
          to="/charts"
          icon={<Map className="h-4 w-4" aria-hidden="true" />}
          label={t('nav.charts')}
          disabled
        />
        <ModuleButton
          to="/settings"
          icon={<Settings className="h-4 w-4" aria-hidden="true" />}
          label={t('nav.settings')}
          disabled
        />
        <div className="flex-1" />
        <ThemeToggle compact />
      </div>
      <CompactBanner />
      <div className="flex min-h-0 flex-1">
        {/* 垂直图标栏（lg+ 布局） */}
        <aside className="hidden w-12 shrink-0 flex-col items-center border-r border-border bg-card py-2 lg:flex">
          <ModuleButton
            to="/"
            icon={<Plane className="h-4 w-4" aria-hidden="true" />}
            label={t('nav.flightplan')}
          />
          <ModuleButton
            to="/charts"
            icon={<Map className="h-4 w-4" aria-hidden="true" />}
            label={t('nav.charts')}
            disabled
          />
          <ModuleButton
            to="/settings"
            icon={<Settings className="h-4 w-4" aria-hidden="true" />}
            label={t('nav.settings')}
            disabled
          />
          <div className="flex-1" />
          <ThemeToggle />
        </aside>
        <main className="min-w-0 flex-1">
          <Outlet />
        </main>
      </div>
    </div>
  );
}
