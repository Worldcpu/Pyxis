// 主题三态切换（S9.1：从图标栏/顶栏移入地图工具栏列；system/light/dark 循环）。

import { MonitorCog, Moon, Sun } from 'lucide-react';
import { useTranslation } from 'react-i18next';

import { useTheme, type Theme } from './theme-provider';
import { Button } from './ui/button';
import { Tooltip, TooltipContent, TooltipTrigger } from './ui/tooltip';

const THEME_CYCLE: Theme[] = ['system', 'light', 'dark'];

export function ThemeToggle() {
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
          aria-label={t('theme.toggleTo', {
            current: label,
            next: t(`theme.${next}`),
          })}
          onClick={() => setTheme(next)}
          className="h-8 w-8"
        >
          <Icon className="h-4 w-4" aria-hidden="true" />
        </Button>
      </TooltipTrigger>
      <TooltipContent>{label}</TooltipContent>
    </Tooltip>
  );
}
