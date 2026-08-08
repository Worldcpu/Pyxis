// 应用壳（决策 32 + S9.1 + 热修复）：垂直图标栏 w-14 贴左占满整高的玻璃
// rail；设置入口沉底（D60）；主题切换在地图工具栏列（D58）；飞行计划
// 图标纯导航（1:1 常驻后无抽屉）。响应式：lg 以下横菜单栏。

import { Map, Plane, Settings, TriangleAlert } from 'lucide-react';
import type { ReactNode } from 'react';
import { NavLink, Outlet } from 'react-router-dom';
import { useTranslation } from 'react-i18next';

import { Button } from './ui/button';
import { Tooltip, TooltipContent, TooltipTrigger } from './ui/tooltip';
import { cn } from '../lib/utils';

/** 模块图标按钮（决策 32：当前模块高亮 G6；占位模块灰显 + 提示）。
    垂直 rail：h-10 按钮 + 18px 图标（贴合 w-14 栏）；顶栏紧凑。 */
function ModuleButton({
  to,
  icon,
  label,
  disabled = false,
  wide = false,
}: {
  to: string;
  icon: ReactNode;
  label: string;
  disabled?: boolean;
  wide?: boolean;
}) {
  const { t } = useTranslation();
  const box = wide ? 'h-10 w-10' : 'h-9 w-9';
  if (disabled) {
    return (
      <Tooltip>
        <TooltipTrigger asChild>
          <Button
            variant="ghost"
            size="icon"
            aria-label={label}
            className={`${box} opacity-40`}
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
              `relative flex ${box} items-center justify-center rounded-md text-muted-foreground transition-colors hover:bg-muted hover:text-foreground`,
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
      <div className="relative min-h-0 flex-1">
        {/* 桌面导航 rail：贴左（left-0）占满整高；右缘 border-r 与任务
            面板左缘贴齐形成分割线；毛玻璃悬浮。 */}
        <aside className="px-glass absolute inset-y-0 left-0 z-30 hidden w-14 flex-col items-center border-r py-3 shadow-xl lg:flex">
          <ModuleButton
            to="/"
            icon={<Plane className="h-[18px] w-[18px]" aria-hidden="true" />}
            label={t('nav.flightplan')}
            wide
          />
          <ModuleButton
            to="/charts"
            icon={<Map className="h-[18px] w-[18px]" aria-hidden="true" />}
            label={t('nav.charts')}
            disabled
            wide
          />
          <div className="flex-1" />
          <ModuleButton
            to="/settings"
            icon={<Settings className="h-[18px] w-[18px]" aria-hidden="true" />}
            label={t('nav.settings')}
            disabled
            wide
          />
        </aside>
        {/* main 全宽：FlightPlanPage 的地图可从 rail 后方延展。 */}
        <main className="relative h-full min-w-0">
          <Outlet />
        </main>
      </div>
    </div>
  );
}
