// Suggest Route 分区（S9.1 D55 rev.）：偏好档案选择（默认 NONE）+
// 生成 5 张横向候选卡；悬停卡高亮地图对应航路 + 临时点标签；与 Route
// 输入归一化匹配的卡显示蓝色 "Route input route" 徽章；点击卡 → 页面
// 处理替换确认（已有规划航路且不同时）。

import { Sparkles } from 'lucide-react';
import { useTranslation } from 'react-i18next';

import type { Profile, RouteCandidate } from '../../api/types';
import { isSameRoute } from '../../lib/route-match';
import { cn } from '../../lib/utils';
import { Button } from '../ui/button';
import { Label } from '../ui/label';
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from '../ui/select';

export interface SuggestRouteProps {
  profiles: Profile[];
  selectedProfile: string;
  onProfileChange: (name: string) => void;
  candidates: RouteCandidate[];
  onGenerate: () => void;
  generating: boolean;
  /** 当前 Route 输入（归一化匹配徽章）。 */
  routeString: string;
  hoveredIndex: number | null;
  onHover: (index: number | null) => void;
  onPick: (candidate: RouteCandidate) => void;
}

export function SuggestRoute({
  profiles,
  selectedProfile,
  onProfileChange,
  candidates,
  onGenerate,
  generating,
  routeString,
  hoveredIndex,
  onHover,
  onPick,
}: SuggestRouteProps) {
  const { t } = useTranslation();
  return (
    <div className="space-y-2">
      <div className="flex items-center justify-between">
        <Label className="text-xs text-muted-foreground">
          {t('suggest.preference')}
        </Label>
        <Select value={selectedProfile} onValueChange={onProfileChange}>
          <SelectTrigger className="w-40">
            <SelectValue placeholder={t('suggest.none')} />
          </SelectTrigger>
          <SelectContent>
            <SelectItem value="">{t('suggest.none')}</SelectItem>
            {profiles.map((p) => (
              <SelectItem key={p.name} value={p.name}>
                {p.name}
              </SelectItem>
            ))}
          </SelectContent>
        </Select>
      </div>
      <Button
        variant="outline"
        size="sm"
        className="w-full"
        onClick={onGenerate}
        disabled={generating}
      >
        <Sparkles className="h-3.5 w-3.5" aria-hidden="true" />
        {generating ? t('suggest.generating') : t('suggest.generate')}
      </Button>
      {candidates.length === 0 ? (
        <p className="text-xs text-muted-foreground">{t('suggest.empty')}</p>
      ) : (
        <div className="flex gap-2 overflow-x-auto pb-1">
          {candidates.map((c) => {
            const matched = isSameRoute(c.route_string, routeString);
            const hovered = hoveredIndex === c.index;
            return (
              <button
                key={c.index}
                type="button"
                onClick={() => onPick(c)}
                onMouseEnter={() => onHover(c.index)}
                onMouseLeave={() => onHover(null)}
                className={cn(
                  'relative flex w-44 shrink-0 flex-col gap-1 rounded-lg border border-border bg-card p-2 text-left hover:border-muted-foreground/50',
                  hovered && 'border-muted-foreground/70',
                  matched && 'border-secondary bg-secondary/10',
                )}
              >
                {matched && (
                  <div className="absolute right-1.5 top-1.5 rounded-md border border-transparent bg-secondary px-1.5 py-0.5 text-[10px] font-medium text-secondary-foreground">
                    {t('suggest.matchBadge')}
                  </div>
                )}
                <span className="line-clamp-2 font-mono text-[11px] leading-tight">
                  {c.route_string}
                </span>
                <span className="font-mono text-[11px] text-muted-foreground">
                  {c.total_distance_nm !== undefined &&
                    `${Math.round(c.total_distance_nm)}NM`}
                </span>
              </button>
            );
          })}
        </div>
      )}
    </div>
  );
}
