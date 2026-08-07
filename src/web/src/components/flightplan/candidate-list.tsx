// ② 候选列表（决策 34/42：SimBrief 式列表主导；点选 → route_string 入状态）。
// 含 seed 显示（G2）；参数过期时顶部橙黄提示（ui-spec §6）。

import { Check, ListChecks } from 'lucide-react';
import { useTranslation } from 'react-i18next';

import type { RouteCandidate } from '../../api/types';
import { cn } from '../../lib/utils';
import { Button } from '../ui/button';

export interface CandidateListProps {
  candidates: RouteCandidate[];
  selectedIndex: number | null;
  onSelect: (index: number) => void;
  onGeneratePlan: () => void;
  loading: boolean;
  seed: number;
  stale: boolean;
}

export function CandidateList({
  candidates,
  selectedIndex,
  onSelect,
  onGeneratePlan,
  loading,
  seed,
  stale,
}: CandidateListProps) {
  const { t } = useTranslation();
  return (
    <div className="flex h-full flex-col">
      {stale && (
        <div className="border-b border-warning/40 bg-warning/10 px-3 py-1.5 text-xs text-warning">
          {t('candidates.expired')}
        </div>
      )}
      <div className="flex items-center justify-between border-b border-border px-3 py-2">
        <span className="flex items-center gap-1.5 text-sm font-semibold">
          <ListChecks className="h-4 w-4" aria-hidden="true" />
          {t('candidates.title')}
        </span>
        {seed > 0 && (
          <span className="font-mono text-xs text-muted-foreground">
            seed={seed}
          </span>
        )}
      </div>
      <div className="min-h-0 flex-1 overflow-y-auto">
        {candidates.length === 0 ? (
          <p className="p-3 text-sm text-muted-foreground">{t('candidates.empty')}</p>
        ) : (
          candidates.map((c) => (
            <button
              key={c.index}
              type="button"
              onClick={() => onSelect(c.index)}
              aria-pressed={selectedIndex === c.index}
              className={cn(
                'w-full border-b border-border px-3 py-2 text-left hover:bg-muted',
                selectedIndex === c.index && 'bg-muted',
              )}
            >
              <div className="flex items-center justify-between">
                <span className="font-mono text-sm">{c.route_string ?? `#${c.index}`}</span>
                {selectedIndex === c.index && (
                  <Check className="h-4 w-4 text-accent" aria-hidden="true" />
                )}
              </div>
              <div className="mt-0.5 flex gap-3 font-mono text-xs text-muted-foreground">
                {c.distance_nm !== undefined && (
                  <span>
                    {t('candidates.distance')} {Math.round(c.distance_nm)}NM
                  </span>
                )}
                {c.altitude?.fl !== undefined && (
                  <span>
                    {t('candidates.fl')} FL{c.altitude.fl}
                  </span>
                )}
              </div>
            </button>
          ))
        )}
      </div>
      <div className="border-t border-border p-3">
        <Button
          className="w-full"
          onClick={onGeneratePlan}
          disabled={loading || selectedIndex === null}
        >
          {loading ? t('candidates.generating') : t('candidates.generatePlan')}
        </Button>
      </div>
    </div>
  );
}
