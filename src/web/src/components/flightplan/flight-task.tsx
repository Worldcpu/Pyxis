// ③ 生成后视图 Flight Task 面板（决策 35-38，整栏纵向滚动）。
// 自上而下：警告条 → 航班任务动线图 + 性能网格 → 舱单四栏 → 气象空态
// → Prefile 三网络 → Flightplan Download。

import {
  Cloud,
  CloudSun,
  Download,
  Droplet,
  ExternalLink,
  Eye,
  Gauge,
  PlaneTakeoff,
  Thermometer,
  TriangleAlert,
  Wind,
  X,
} from 'lucide-react';
import { useTranslation } from 'react-i18next';
import { useState, type ReactNode } from 'react';

import type { Airframe, FlightPlan } from '../../api/types';
import { cn } from '../../lib/utils';
import { formatAlt } from '../../lib/units';
import { buildPilotEdgeUrl, buildVatsimUrl, buildIvaoUrl } from '../../lib/prefile';
import type { PlanFormState } from './form-state';
import { Badge } from '../ui/badge';
import { Button } from '../ui/button';
import { Switch } from '../ui/switch';
import {
  Collapsible,
  CollapsibleContent,
  CollapsibleTrigger,
} from '../ui/collapsible';
import { Tooltip, TooltipContent, TooltipTrigger } from '../ui/tooltip';

export type WarningKey = 'unflyable' | 'manualRoute' | 'staleCandidates';

export interface FlightTaskProps {
  plan: FlightPlan;
  form: PlanFormState;
  airframe: Airframe | null;
  cycles: number[];
  stale: boolean;
  dismissedWarnings: WarningKey[];
  onDismissWarning: (key: WarningKey) => void;
  onExport: () => void;
  exporting: boolean;
}

/** 性能网格格（上标题 + 下数值，三列居中）。 */
function Metric({ label, value }: { label: string; value: ReactNode }) {
  return (
    <div className="flex flex-col items-center gap-0.5 px-1">
      <span className="text-[11px] uppercase tracking-wide text-muted-foreground">
        {label}
      </span>
      <span className="font-mono text-sm tabular-nums">{value}</span>
    </div>
  );
}

/** 舱单四栏（决策 20：Passengers/Baggage 前端自持，Burn/TOW/LW Phase 10 `--`）。 */
function OfpSection({ plan, form }: { plan: FlightPlan; form: PlanFormState }) {
  const { t } = useTranslation();
  const rows: Array<[string, string, string, string]> = [
    [t('task.ofp.enrouteBurn'), '--', t('task.ofp.blockBurn'), '--'],
    [
      t('task.ofp.passengers'),
      form.paxCount || '0',
      t('task.ofp.baggage'),
      `${form.cargoKg || '0'} kg`,
    ],
    [
      t('task.ofp.payload'),
      '--',
      t('task.ofp.zfw'),
      `${plan.weights?.zfw_kg ?? '--'} kg`,
    ],
    [t('task.ofp.tow'), '--', t('task.ofp.lw'), '--'],
  ];
  return (
    <table className="w-full font-mono text-sm">
      <tbody>
        {rows.map(([l1, v1, l2, v2]) => (
          <tr key={l1} className="border-b border-border/60">
            <td className="py-1 text-xs text-muted-foreground">{l1}</td>
            <td className="py-1 text-right tabular-nums">{v1}</td>
            <td className="py-1 pl-4 text-xs text-muted-foreground">{l2}</td>
            <td className="py-1 text-right tabular-nums">{v2}</td>
          </tr>
        ))}
      </tbody>
    </table>
  );
}

/** 气象六宫格空态（决策 37：Phase 11 数据；-- 占位）。 */
function WeatherCell({ icon, label }: { icon: ReactNode; label: string }) {
  return (
    <div className="flex flex-col items-center gap-1 rounded border border-border p-1.5">
      {icon}
      <span className="text-[11px] text-muted-foreground">{label}</span>
      <span className="font-mono text-sm">--</span>
    </div>
  );
}

function WeatherSection({ plan }: { plan: FlightPlan }) {
  const { t } = useTranslation();
  const [raw, setRaw] = useState(false);
  const airports = [
    plan.route.points[0]?.ident ?? '--',
    plan.route.points[plan.route.points.length - 1]?.ident ?? '--',
  ];
  return (
    <div className="space-y-2">
      <div className="flex items-center justify-between">
        <span className="flex items-center gap-1.5 text-sm font-semibold">
          <Cloud className="h-4 w-4" aria-hidden="true" />
          {t('task.weather.title')}
        </span>
      </div>
      {airports.map((icao, i) => (
        <div key={`${icao}-${i}`} className="space-y-1.5 rounded border border-border p-2">
          <div className="flex items-center justify-between">
            <span className="font-mono text-sm font-semibold">{icao}</span>
            <span className="flex items-center gap-1.5">
              <Badge variant="outline">{t('task.weather.category.vfr')}</Badge>
              <label className="flex items-center gap-1 text-xs text-muted-foreground">
                {t('task.weather.rawData')}
                <Switch checked={raw} onCheckedChange={setRaw} />
              </label>
            </span>
          </div>
          <div className="grid grid-cols-3 gap-1.5">
            <WeatherCell icon={<Eye className="h-3.5 w-3.5 text-muted-foreground" aria-hidden="true" />} label={t('task.weather.fields.visibility')} />
            <WeatherCell icon={<Gauge className="h-3.5 w-3.5 text-muted-foreground" aria-hidden="true" />} label={t('task.weather.fields.qnh')} />
            <WeatherCell icon={<Wind className="h-3.5 w-3.5 text-muted-foreground" aria-hidden="true" />} label={t('task.weather.fields.wind')} />
            <WeatherCell icon={<Thermometer className="h-3.5 w-3.5 text-muted-foreground" aria-hidden="true" />} label={t('task.weather.fields.temp')} />
            <WeatherCell icon={<Droplet className="h-3.5 w-3.5 text-muted-foreground" aria-hidden="true" />} label={t('task.weather.fields.dewpoint')} />
            <WeatherCell icon={<CloudSun className="h-3.5 w-3.5 text-muted-foreground" aria-hidden="true" />} label={t('task.weather.fields.tropopause')} />
          </div>
          {raw && (
            <p className="rounded bg-muted p-1.5 font-mono text-xs text-muted-foreground">
              {t('task.weather.placeholder')}
            </p>
          )}
        </div>
      ))}
    </div>
  );
}

/** Prefile 三网络（决策 38：默认折叠；巡航速度取自 airframe 档案）。 */
function PrefileSection({
  plan,
  form,
  airframe,
}: {
  plan: FlightPlan;
  form: PlanFormState;
  airframe: Airframe | null;
}) {
  const { t } = useTranslation();
  const speed = airframe?.cruise_speed_kt;
  const base = {
    callsign: form.callsign || plan.callsign || 'PYXIS',
    departure: plan.route.points[0]?.ident ?? '',
    arrival: plan.route.points[plan.route.points.length - 1]?.ident ?? '',
    etd: form.etd || '0000',
    cruiseFl: plan.altitude.fl,
    cruiseSpeedKt: speed ?? 0,
    airframeType: airframe?.type ?? '',
    routeString: form.routeString,
    alternate: plan.alternate || form.alternate,
  };
  const networks = [
    { name: 'VATSIM', website: 'vatsim.net', url: buildVatsimUrl(base) },
    { name: 'IVAO', website: 'ivao.aero', url: buildIvaoUrl(base) },
    { name: 'PilotEdge', website: 'pilotedge.net', url: buildPilotEdgeUrl(base) },
  ];
  return (
    <div className="space-y-2">
      {networks.map((n) => (
        <div key={n.name} className="grid grid-cols-[1fr_1fr_auto] items-center gap-2">
          <span className="text-sm">{n.name}</span>
          <span className="truncate font-mono text-xs text-muted-foreground">
            {n.website}
          </span>
          <Tooltip>
            <TooltipTrigger asChild>
              <span>
                <Button
                  variant="outline"
                  size="sm"
                  disabled={!speed || speed <= 0}
                  onClick={() => window.open(n.url, '_blank')}
                >
                  <ExternalLink className="h-3.5 w-3.5" aria-hidden="true" />
                  {t('task.prefile.prefile')}
                </Button>
              </span>
            </TooltipTrigger>
            <TooltipContent>
              {speed && speed > 0
                ? t('task.prefile.openInBrowser')
                : t('task.prefile.noCruiseSpeed')}
            </TooltipContent>
          </Tooltip>
        </div>
      ))}
    </div>
  );
}

export function FlightTask({
  plan,
  form,
  airframe,
  cycles,
  stale,
  dismissedWarnings,
  onDismissWarning,
  onExport,
  exporting,
}: FlightTaskProps) {
  const { t } = useTranslation();
  const warnings: Array<{ key: WarningKey; tone: 'error' | 'warning' }> = [];
  if (plan.checks?.status === 'unflyable') {
    warnings.push({ key: 'unflyable', tone: 'error' });
  }
  if (plan.mora_checked === false) {
    warnings.push({ key: 'manualRoute', tone: 'warning' });
  }
  if (stale) {
    warnings.push({ key: 'staleCandidates', tone: 'warning' });
  }
  const visible = warnings.filter((w) => !dismissedWarnings.includes(w.key));

  const dep = plan.route.points[0];
  const arr = plan.route.points[plan.route.points.length - 1];
  const enroute_nm = plan.distances?.enroute_nm ?? plan.distance_nm;
  const etd = form.etd || '--';

  return (
    <div className="h-full overflow-y-auto">
      {/* 警告条（决策 36：三类；x 关闭本次会话消失） */}
      {visible.map((w) => (
        <div
          key={w.key}
          role="alert"
          className={cn(
            'flex items-center gap-2 px-3 py-2 text-xs',
            w.tone === 'error'
              ? 'bg-error/10 text-error'
              : 'bg-warning/10 text-warning',
          )}
        >
          <TriangleAlert className="h-4 w-4 shrink-0" aria-hidden="true" />
          <span className="flex-1">{t(`task.warnings.${w.key}`)}</span>
          <button
            type="button"
            aria-label={t('task.closeWarning')}
            onClick={() => onDismissWarning(w.key)}
            className="opacity-70 hover:opacity-100"
          >
            <X className="h-3.5 w-3.5" aria-hidden="true" />
          </button>
        </div>
      ))}

      {/* 航班任务（决策 35：标题 + 动线图 + 性能网格） */}
      <div className="space-y-3 p-3">
        <div className="flex items-center justify-between">
          <span className="text-base font-semibold">{t('task.title')}</span>
          <span className="font-mono text-sm text-muted-foreground">
            {form.callsign || plan.callsign || '--'}
          </span>
        </div>

        {/* 动线图（wheels 口径：离地/触地时刻） */}
        <div className="rounded border border-border p-3">
          <div className="flex items-center">
            <div className="flex flex-col items-center">
              <span className="font-mono text-lg font-semibold">
                {dep?.ident ?? '--'}
              </span>
              <span className="font-mono text-xs text-muted-foreground">
                {etd}Z
              </span>
            </div>
            <div className="mx-2 flex flex-1 items-center">
              <div className="h-px flex-1 border-t border-dashed border-muted-foreground/50" />
              <PlaneTakeoff
                className="mx-1 h-3.5 w-3.5 text-muted-foreground"
                aria-hidden="true"
              />
              <div className="h-px flex-1 border-t border-dashed border-muted-foreground/50" />
            </div>
            <div className="flex flex-col items-center">
              <span className="font-mono text-lg font-semibold">
                {arr?.ident ?? '--'}
              </span>
              <span className="font-mono text-xs text-muted-foreground">--Z</span>
            </div>
          </div>
          <div className="mt-1.5 flex justify-center gap-4 font-mono text-xs text-muted-foreground">
            <span>
              {t('task.airTime')} --
            </span>
            <span>
              {t('task.distance')}{' '}
              {enroute_nm !== undefined ? `${Math.round(enroute_nm)}NM` : '--'}
            </span>
          </div>
        </div>

        {/* 性能网格（决策 35：六项三列） */}
        <div className="grid grid-cols-3 gap-y-2 rounded border border-border p-2">
          <Metric
            label={t('task.performance.alternate')}
            value={plan.alternate || form.alternate || '--'}
          />
          <Metric
            label={t('task.performance.costIndex')}
            value={form.costIndex || '0'}
          />
          <Metric
            label={t('task.performance.zfw')}
            value={`${plan.weights?.zfw_kg ?? '--'} kg`}
          />
          <Metric label={t('task.performance.blockTime')} value="--" />
          <Metric
            label={t('task.performance.cruise')}
            value={formatAlt(plan.altitude.fl)}
          />
          <Metric
            label={t('task.performance.airac')}
            value={cycles[0] !== undefined ? String(cycles[0]) : '--'}
          />
        </div>
      </div>

      {/* 舱单（决策 20） */}
      <div className="border-t border-border p-3">
        <OfpSection plan={plan} form={form} />
      </div>

      {/* 航务气象（决策 37：空态接口位） */}
      <div className="border-t border-border p-3">
        <WeatherSection plan={plan} />
      </div>

      {/* Prefile（决策 38：默认折叠） */}
      <div className="border-t border-border">
        <Collapsible>
          <CollapsibleTrigger className="flex w-full items-center justify-between px-3 py-2 text-sm font-semibold hover:bg-muted">
            {t('task.prefile.title')}
          </CollapsibleTrigger>
          <CollapsibleContent className="space-y-2 px-3 pb-3">
            <PrefileSection plan={plan} form={form} airframe={airframe} />
          </CollapsibleContent>
        </Collapsible>
      </div>

      {/* Download（决策 17：默认折叠） */}
      <div className="border-t border-border">
        <Collapsible>
          <CollapsibleTrigger className="flex w-full items-center justify-between px-3 py-2 text-sm font-semibold hover:bg-muted">
            {t('task.download.title')}
          </CollapsibleTrigger>
          <CollapsibleContent className="space-y-2 px-3 pb-3">
            <div className="flex items-center justify-between">
              <span className="text-xs text-muted-foreground">
                {t('task.download.format')}
              </span>
              <span className="font-mono text-sm">.PLN</span>
            </div>
            <Button
              className="w-full"
              variant="outline"
              onClick={onExport}
              disabled={exporting}
            >
              <Download className="h-3.5 w-3.5" aria-hidden="true" />
              {exporting
                ? t('task.download.downloading')
                : t('task.download.download')}
            </Button>
          </CollapsibleContent>
        </Collapsible>
      </div>
    </div>
  );
}
