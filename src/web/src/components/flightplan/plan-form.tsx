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
  /** Suggest Route（D55 rev.）。 */
  profiles: Profile[];
  selectedProfile: string;
  onProfileChange: (name: string) => void;
  candidates: RouteCandidate[];
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
  const [touched, setTouched] = useState<{
    departure?: boolean;
    arrival?: boolean;
  }>({});
  const icaoErrors = {
    departure: touched.departure ? icaoError(value.departure) : undefined,
    arrival: touched.arrival ? icaoError(value.arrival) : undefined,
  };

  return (
    <div className="flex min-h-full flex-col">
      {/* Flight Info（D57：跑道字段移除，随 METAR 回归） */}
      <Section title={t('form.flightInfo')}>
        <div className="grid grid-cols-2 gap-2">
          <Field label={t('form.callsign')}>
            <Input
              value={value.callsign}
              placeholder={t('form.callsignPlaceholder')}
              onChange={(e) => set({ callsign: e.target.value.toUpperCase() })}
            />
          </Field>
          <Field label={t('form.etd')}>
            <Input
              value={value.etd}
              placeholder="1040"
              onChange={(e) =>
                set({ etd: e.target.value.replace(/\D/g, '').slice(0, 4) })
              }
            />
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
              <SelectContent>
                {variants.map((a) => (
                  <SelectItem key={a.variant} value={a.variant}>
                    <span className="flex items-center gap-1.5">
                      {a.variant}
                      {experimental.has(a.perf_source) && (
                        <Badge variant="outline">{t('form.experimental')}</Badge>
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
            onValueChange={(v) => set({ alternate: v })}
          >
            <SelectTrigger className="w-40">
              <SelectValue placeholder={t('form.noAlternate')} />
            </SelectTrigger>
            <SelectContent>
              <SelectItem value="">{t('form.noAlternate')}</SelectItem>
              {alternates.map((a) => (
                <SelectItem key={a.icao} value={a.icao}>
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

      {/* Optional Entries（决策 7；Phase 10 展开）——D53：仅此可折叠 */}
      <Section title={t('form.optional')} collapsible defaultOpen={false}>
        <p className="text-xs text-muted-foreground">{t('form.optionalHint')}</p>
      </Section>

      {/* Fuel Planning（决策 11/21；数值 Phase 10 填充）——D53：仅此可折叠 */}
      <Section title={t('form.fuel')} collapsible defaultOpen={false}>
        <p className="text-xs text-muted-foreground">{t('form.fuelHint')}</p>
      </Section>

      {/* Route（D54：大输入框 + 分析 + 有效性反馈） */}
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
      </Section>

      {/* Suggest Route（D55 rev.：偏好 + 候选卡；常展核心分区） */}
      <Section title={t('suggest.title')}>
        <SuggestRoute
          profiles={profiles}
          selectedProfile={selectedProfile}
          onProfileChange={onProfileChange}
          candidates={candidates}
          onGenerate={onGenerateCandidates}
          generating={generatingCandidates}
          routeString={value.routeString}
          hoveredIndex={hoveredCandidateIndex}
          onHover={onHoverCandidate}
          onPick={onPickCandidate}
        />
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
