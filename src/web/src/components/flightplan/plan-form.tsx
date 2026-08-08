// ① 生成前表单（ui-spec §3 + S9.1 D53/D54/D57）：核心分区常展（Flight
// Info/Aircraft/Selections/Route），仅 Optional/Fuel 折叠；Route 分区 = 大
// 输入框 + 分析按钮 + 有效性反馈（plan.analyze）；底部生成按钮（两阶段
// 流程 D56，enabled iff 存在有效规划航路）。跑道字段已移除（D57：随 METAR
// 回归）。

import { ChevronDown } from 'lucide-react';
import { useTranslation } from 'react-i18next';
import { useState, type ReactNode } from 'react';

import type {
  Airframe,
  Alternate,
  AnalyzeResult,
  Profile,
  RouteCandidate,
} from '../../api/types';
import { Button } from '../ui/button';
import { Input } from '../ui/input';
import { Label } from '../ui/label';
import { Badge } from '../ui/badge';
import { SuggestRoute } from './suggest-route';
import {
  Collapsible,
  CollapsibleContent,
  CollapsibleTrigger,
} from '../ui/collapsible';
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from '../ui/select';
import type { PlanFormState } from './form-state';

/** 分区（D53：核心分区无折叠头直接常展；Optional/Fuel 用 collapsible）。 */
function Section({
  title,
  children,
  collapsible = false,
  defaultOpen = true,
}: {
  title: string;
  children: ReactNode;
  collapsible?: boolean;
  defaultOpen?: boolean;
}) {
  if (!collapsible) {
    return (
      <div className="border-b border-border">
        <div className="px-3 py-2 text-sm font-semibold">{title}</div>
        <div className="space-y-2 px-3 pb-3">{children}</div>
      </div>
    );
  }
  return (
    <Collapsible defaultOpen={defaultOpen} className="border-b border-border">
      <CollapsibleTrigger className="flex w-full items-center justify-between px-3 py-2 text-sm font-semibold hover:bg-muted">
        {title}
        <ChevronDown className="h-3.5 w-3.5 opacity-50" aria-hidden="true" />
      </CollapsibleTrigger>
      <CollapsibleContent className="space-y-2 px-3 pb-3">
        {children}
      </CollapsibleContent>
    </Collapsible>
  );
}

function Field({
  label,
  children,
  error,
}: {
  label: string;
  children: ReactNode;
  error?: string;
}) {
  return (
    <div className="space-y-1">
      <Label className="text-xs text-muted-foreground">{label}</Label>
      {children}
      {error && (
        <p role="alert" className="text-xs text-error">
          {error}
        </p>
      )}
    </div>
  );
}

/** 4 字 ICAO 校验（决策 12 修订口径；onBlur 触发）。 */
function icaoError(v: string): string | undefined {
  const t = v.trim().toUpperCase();
  if (!t) return undefined;
  return /^[A-Z0-9]{4}$/.test(t) ? undefined : '需为 4 字 ICAO';
}

export interface PlanFormProps {
  value: PlanFormState;
  onChange: (next: PlanFormState) => void;
  airframes: Airframe[];
  alternates: Alternate[];
  /** Route 分析（D54）：plan.analyze 调用方。 */
  onAnalyzeRoute: () => void;
  analyzing: boolean;
  analyzed: AnalyzeResult | null;
  /** 底部生成计划（D56 两阶段）。 */
  onGeneratePlan: () => void;
  generating: boolean;
  canGenerate: boolean;
  /** 备降菜单项 hover → 地图联动（grill 确认：仅显示该项 + 到达场虚线）。 */
  onAlternateHover: (icao: string | null) => void;
  /** Suggest Route（D55 rev. + 合并：候选区在 Route 分区内）。 */
  profiles: Profile[];
  selectedProfile: string;
  onProfileChange: (name: string) => void;
  candidates: RouteCandidate[];
  /** 候选 seed（D42：换一批 = seed+1）。 */
  candidateSeed: number;
  onGenerateCandidates: () => void;
  generatingCandidates: boolean;
  hoveredCandidateIndex: number | null;
  onHoverCandidate: (index: number | null) => void;
  onPickCandidate: (candidate: RouteCandidate) => void;
}

export function PlanForm({
  value,
  onChange,
  airframes,
  alternates,
  onAlternateHover,
  onAnalyzeRoute,
  analyzing,
  analyzed,
  onGeneratePlan,
  generating,
  canGenerate,
  profiles,
  selectedProfile,
  onProfileChange,
  candidates,
  candidateSeed,
  onGenerateCandidates,
  generatingCandidates,
  hoveredCandidateIndex,
  onHoverCandidate,
  onPickCandidate,
}: PlanFormProps) {
  const { t } = useTranslation();
  const set = (patch: Partial<PlanFormState>) => onChange({ ...value, ...patch });

  // 机型的经验/实验性分类（决策 8 徽章）。
  const experimental = new Set(['openap', 'fcom']);
  const airframeByType = airframes.filter((a) => a.type === value.airframeType);
  // 审查修复：未选 type 时 variant 禁用（此前回退列出全部机型变体，
  // 产生 (type, variant) 不一致组合且生成被泛化错误阻塞）。
  const variants = value.airframeType ? airframeByType : [];

  // 审查修复：touched 状态驱动派生校验（此前缓存错误快照——SimBrief
  // 导入/候选回填写入不触发 onBlur → 错误残留）。
  // Flight Info 全必填（用户要求）：呼号/EOBT/起降场缺一不可。
  const required = (v: string): string | undefined =>
    v.trim() ? undefined : t('form.required');
  const [touched, setTouched] = useState<{
    callsign?: boolean;
    etd?: boolean;
    departure?: boolean;
    arrival?: boolean;
  }>({});
  const icaoErrors = {
    callsign: touched.callsign ? required(value.callsign) : undefined,
    etd: touched.etd ? required(value.etd) : undefined,
    departure: touched.departure
      ? (required(value.departure) ?? icaoError(value.departure))
      : undefined,
    arrival: touched.arrival
      ? (required(value.arrival) ?? icaoError(value.arrival))
      : undefined,
  };

  return (
    <div className="flex min-h-full flex-col">
      {/* Flight Info（D57：跑道字段移除，随 METAR 回归） */}
      <Section title={t('form.flightInfo')}>
        <div className="grid grid-cols-2 gap-2">
          <Field label={t('form.callsign')} error={icaoErrors.callsign}>
            <Input
              value={value.callsign}
              placeholder={t('form.callsignPlaceholder')}
              onChange={(e) => set({ callsign: e.target.value.toUpperCase() })}
              onBlur={() => setTouched((p) => ({ ...p, callsign: true }))}
            />
          </Field>
          <Field label={t('form.etd')} error={icaoErrors.etd}>
            {/* EOBT（SimBrief 式日期+时间选择；值格式 YYYY-MM-DDTHH:mm，
                语义为 Zulu——datetime-local 原样显示字符串无时区偏移） */}
            <Input
              className="min-w-0 [color-scheme:light] dark:[color-scheme:dark]"
              type="datetime-local"
              value={value.etd}
              onChange={(e) => set({ etd: e.target.value })}
              onBlur={() => setTouched((p) => ({ ...p, etd: true }))}
            />
            <p className="text-[11px] text-muted-foreground">
              {t('form.etdHint')}
            </p>
          </Field>
          <Field label={t('form.departure')} error={icaoErrors.departure}>
            <Input
              value={value.departure}
              placeholder={t('form.departurePlaceholder')}
              onChange={(e) => set({ departure: e.target.value.toUpperCase() })}
              onBlur={() => setTouched((p) => ({ ...p, departure: true }))}
            />
          </Field>
          <Field label={t('form.arrival')} error={icaoErrors.arrival}>
            <Input
              value={value.arrival}
              placeholder={t('form.arrivalPlaceholder')}
              onChange={(e) => set({ arrival: e.target.value.toUpperCase() })}
              onBlur={() => setTouched((p) => ({ ...p, arrival: true }))}
            />
          </Field>
        </div>
      </Section>

      {/* Aircraft：两级选择器 + 实验性徽章 */}
      <Section title={t('form.aircraft')}>
        <div className="grid grid-cols-2 gap-2">
          <Field label={t('form.type')}>
            <Select
              value={value.airframeType}
              onValueChange={(v) => set({ airframeType: v, airframeVariant: '' })}
            >
              <SelectTrigger>
                <SelectValue placeholder="—" />
              </SelectTrigger>
              <SelectContent>
                {Array.from(new Set(airframes.map((a) => a.type))).map((ty) => (
                  <SelectItem key={ty} value={ty}>
                    {ty}
                  </SelectItem>
                ))}
              </SelectContent>
            </Select>
          </Field>
          <Field label={t('form.variant')}>
            <Select
              value={value.airframeVariant}
              onValueChange={(v) => set({ airframeVariant: v })}
            >
              <SelectTrigger>
                <SelectValue placeholder="—" />
              </SelectTrigger>
              <SelectContent className="min-w-[12rem]">
                {variants.map((a) => (
                  <SelectItem key={a.variant} value={a.variant}>
                    <span className="flex min-w-0 items-center gap-1.5">
                      <span className="truncate">{a.variant}</span>
                      {experimental.has(a.perf_source) && (
                        <Badge variant="outline" className="shrink-0">
                          {t('form.experimental')}
                        </Badge>
                      )}
                    </span>
                  </SelectItem>
                ))}
              </SelectContent>
            </Select>
          </Field>
        </div>
      </Section>

      {/* Selections */}
      <Section title={t('form.selections')}>
        <div className="grid grid-cols-3 gap-2">
          <Field label={t('form.paxCount')}>
            <Input
              type="number"
              min={0}
              value={value.paxCount}
              disabled={value.payloadMode === 'zfw'}
              onChange={(e) => set({ paxCount: e.target.value })}
            />
          </Field>
          <Field label={t('form.cargoKg')}>
            <Input
              type="number"
              min={0}
              value={value.cargoKg}
              disabled={value.payloadMode === 'zfw'}
              onChange={(e) => set({ cargoKg: e.target.value })}
            />
          </Field>
          <Field label={t('form.zfwKg')}>
            <Input
              type="number"
              min={0}
              value={value.zfwKg}
              disabled={value.payloadMode === 'pax'}
              onChange={(e) => set({ zfwKg: e.target.value })}
            />
          </Field>
        </div>
        <p className="text-xs text-muted-foreground">{t('form.payloadHint')}</p>
        <div className="flex items-center justify-between">
          <Label className="text-xs text-muted-foreground">
            {t('form.alternate')}
          </Label>
          <Select
            value={value.alternate}
            onValueChange={(v) => {
              set({ alternate: v });
              onAlternateHover(v || null); // 选择锁定地图联动
            }}
          >
            <SelectTrigger className="w-40">
              <SelectValue placeholder={t('form.noAlternate')} />
            </SelectTrigger>
            <SelectContent>
              {/* NONE 特殊处理：不联动地图（保持全部橙色方块显示） */}
              <SelectItem
                value=""
                onMouseEnter={() => onAlternateHover(null)}
                onMouseLeave={() => onAlternateHover(null)}
              >
                {t('form.noAlternate')}
              </SelectItem>
              {alternates.map((a) => (
                <SelectItem
                  key={a.icao}
                  value={a.icao}
                  onMouseEnter={() => onAlternateHover(a.icao)}
                  onMouseLeave={() => onAlternateHover(null)}
                >
                  {a.icao} · {Math.round(a.distance_nm)}NM
                </SelectItem>
              ))}
            </SelectContent>
          </Select>
        </div>
        <div className="flex items-center justify-between">
          <Label className="text-xs text-muted-foreground">
            {t('form.costIndex')}
          </Label>
          <Input
            type="number"
            min={0}
            className="w-24"
            value={value.costIndex}
            onChange={(e) => set({ costIndex: e.target.value })}
          />
        </div>
      </Section>

      {/* Fuel Planning（决策 11/21；数值 Phase 10 填充）——D53：仅此可折叠 */}
      <Section title={t('form.fuel')} collapsible defaultOpen={false}>
        <p className="text-xs text-muted-foreground">{t('form.fuelHint')}</p>
      </Section>

      {/* Route（D54 + 合并：大输入框 + 分析 + 有效性反馈 + 候选区——
          偏好/生成·换一批/竖栏候选卡） */}
      <Section title={t('form.route')}>
        <textarea
          aria-label={t('form.routeString')}
          className="h-24 w-full resize-none rounded-md border border-input bg-background px-3 py-2 font-mono text-sm focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-ring"
          value={value.routeString}
          placeholder={t('form.routePlaceholder')}
          onChange={(e) => set({ routeString: e.target.value.toUpperCase() })}
        />
        <Button
          variant="outline"
          size="sm"
          type="button"
          onClick={onAnalyzeRoute}
          disabled={analyzing || value.routeString.trim() === ''}
        >
          {analyzing ? t('form.analyzing') : t('form.analyzeRoute')}
        </Button>
        {analyzed && analyzed.valid && (
          <p
            role="status"
            className="rounded-md border border-success/40 bg-success/10 px-3 py-1.5 text-xs text-success"
          >
            {t('form.analyzeSuccess', {
              cycle: analyzed.cycle,
              distance: Math.round(analyzed.distance_nm ?? 0),
            })}
          </p>
        )}
        {analyzed && !analyzed.valid && (
          <ul
            role="alert"
            className="space-y-1 rounded-md border border-error/40 bg-error/10 px-3 py-1.5 text-xs text-error"
          >
            {(analyzed.errors ?? [{ message: t('errors.parseError') }]).map(
              (e, i) => (
                <li key={i}>{e.message}</li>
              ),
            )}
          </ul>
        )}
        {/* 候选区（合并进 Route：偏好 + 生成/换一批 + 竖栏） */}
        <SuggestRoute
          profiles={profiles}
          selectedProfile={selectedProfile}
          onProfileChange={onProfileChange}
          candidates={candidates}
          seed={candidateSeed}
          onGenerate={onGenerateCandidates}
          generating={generatingCandidates}
          routeString={value.routeString}
          hoveredIndex={hoveredCandidateIndex}
          onHover={onHoverCandidate}
          onPick={onPickCandidate}
        />
      </Section>

      {/* Route Finder（grill 确认：高级航路编辑器——途经/避让点 + 高度带，
          放页面最后；折叠启用——高级选项渐进披露） */}
      <Section title={t('routeFinder.title')} collapsible defaultOpen={false}>
        <Field label={t('routeFinder.forcedPoints')}>
          <Input
            className="font-mono"
            value={value.forcedPoints}
            placeholder={t('routeFinder.forcedHint')}
            onChange={(e) => set({ forcedPoints: e.target.value.toUpperCase() })}
          />
        </Field>
        <Field label={t('routeFinder.avoidWaypoints')}>
          <Input
            className="font-mono"
            value={value.avoidWaypoints}
            placeholder={t('routeFinder.avoidHint')}
            onChange={(e) =>
              set({ avoidWaypoints: e.target.value.toUpperCase() })
            }
          />
        </Field>
        <div className="flex items-center justify-between">
          <Label className="text-xs text-muted-foreground">
            {t('routeFinder.flBand')}
          </Label>
          <div className="flex items-center gap-1">
            <Input
              type="number"
              min={0}
              className="w-20"
              value={value.minFl}
              onChange={(e) => set({ minFl: e.target.value })}
            />
            <span className="text-xs text-muted-foreground">—</span>
            <Input
              type="number"
              min={0}
              className="w-20"
              value={value.maxFl}
              onChange={(e) => set({ maxFl: e.target.value })}
            />
          </div>
        </div>
      </Section>

      {/* 底部生成（D56 两阶段：enabled iff 存在有效规划航路） */}
      <div className="sticky bottom-0 mt-auto border-t border-border bg-background/75 p-3 backdrop-blur-md">
        <Button
          className="w-full"
          onClick={onGeneratePlan}
          disabled={generating || !canGenerate}
        >
          {generating ? t('form.generating') : t('form.generatePlan')}
        </Button>
      </div>
    </div>
  );
}
