// 应用壳（决策 32 + S9.1）：垂直图标栏 w-14 玻璃浮层；设置入口移入
// 图标栏底部（D60）；主题切换移入地图工具栏列（D58）。响应式（决策 32）：
// lg(1024px) 以下降级为横菜单栏 + 顶部横幅。

import { Map, Plane, Settings, TriangleAlert } from 'lucide-react';
import type { ReactNode } from 'react';
import { NavLink, Outlet } from 'react-router-dom';
import { useTranslation } from 'react-i18next';

import { Button } from './ui/button';
import { Tooltip, TooltipContent, TooltipTrigger } from './ui/tooltip';
import { cn } from '../lib/utils';

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
      {/* 横菜单栏（<lg 降级布局；主题切换在地图工具栏列，D58） */}
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
      </div>
      <CompactBanner />
      <div className="flex min-h-0 flex-1">
        {/* 垂直图标栏（lg+ 布局；S9.1：w-14 玻璃浮层 + 设置沉底 D60） */}
        <aside className="hidden w-14 shrink-0 flex-col items-center border-r border-border bg-background/75 py-2 backdrop-blur-md lg:flex">
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
          <div className="flex-1" />
          <ModuleButton
            to="/settings"
            icon={<Settings className="h-4 w-4" aria-hidden="true" />}
            label={t('nav.settings')}
            disabled
          />
        </aside>
        <main className="relative min-w-0 flex-1">
          <Outlet />
        </main>
      </div>
    </div>
  );
}
