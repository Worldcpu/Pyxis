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
  /** 候选 seed（D42：显示 + 换一批 = seed+1）。 */
  seed: number;
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
  seed,
  onGenerate,
  generating,
  routeString,
  hoveredIndex,
  onHover,
  onPick,
}: SuggestRouteProps) {
  const { t } = useTranslation();
  const hasCandidates = candidates.length > 0;
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
        {generating
          ? t('suggest.generating')
          : hasCandidates
            ? t('suggest.reseed')
            : t('suggest.generate')}
      </Button>
      {seed > 0 && (
        <p className="text-right font-mono text-[11px] text-muted-foreground">
          seed={seed}
        </p>
      )}
      {candidates.length === 0 ? (
        <p className="text-xs text-muted-foreground">{t('suggest.empty')}</p>
      ) : (
        <ul className="space-y-1.5">
          {candidates.map((c) => {
            const matched = isSameRoute(c.route_string, routeString);
            const hovered = hoveredIndex === c.index;
            return (
              <li key={c.index}>
                <button
                  type="button"
                  onClick={() => onPick(c)}
                  onMouseEnter={() => onHover(c.index)}
                  onMouseLeave={() => onHover(null)}
                  className={cn(
                    'flex w-full items-center justify-between gap-2 rounded-lg border border-border bg-card px-2.5 py-2 text-left hover:border-muted-foreground/50',
                    hovered && 'border-muted-foreground/70',
                    matched && 'border-secondary bg-secondary/10',
                  )}
                >
                  <span className="line-clamp-2 min-w-0 flex-1 font-mono text-[11px] leading-tight">
                    {c.route_string}
                  </span>
                  <span className="flex shrink-0 items-center gap-1.5">
                    {matched && (
                      <span className="rounded-md bg-secondary px-1.5 py-0.5 text-[10px] font-medium text-secondary-foreground">
                        {t('suggest.matchBadge')}
                      </span>
                    )}
                    <span className="font-mono text-[11px] text-muted-foreground">
                      {c.total_distance_nm !== undefined &&
                        `${Math.round(c.total_distance_nm)}NM`}
                    </span>
                  </span>
                </button>
              </li>
            );
          })}
        </ul>
      )}
    </div>
  );
}
