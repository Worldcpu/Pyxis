// ① 生成前表单（ui-spec §3：六分区 SimBrief 式折叠组）。
// 内联校验（onBlur + role=alert，G7/G8）、渐进披露、SimBrief 导入按钮。

import { ChevronDown, Import } from 'lucide-react';
import { useTranslation } from 'react-i18next';
import { useState, type ReactNode } from 'react';

import type { Airframe, Alternate } from '../../api/types';
import { Button } from '../ui/button';
import { Input } from '../ui/input';
import { Label } from '../ui/label';
import { Badge } from '../ui/badge';
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

/** 折叠分区标题（SimBrief 式：左标题 + 右箭头）。 */
function Section({
  title,
  children,
  defaultOpen = true,
}: {
  title: string;
  children: ReactNode;
  defaultOpen?: boolean;
}) {
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
  onRefreshAlternates: () => void;
  onGenerateCandidates: () => void;
  loading: boolean;
  onImportSimBrief: (html: string) => void;
}

export function PlanForm({
  value,
  onChange,
  airframes,
  alternates,
  onGenerateCandidates,
  loading,
  onImportSimBrief,
}: PlanFormProps) {
  const { t } = useTranslation();
  const set = (patch: Partial<PlanFormState>) => onChange({ ...value, ...patch });

  // 机型的经验/实验性分类（决策 8 徽章）。
  const experimental = new Set(['openap', 'fcom']);
  const airframeByType = airframes.filter((a) => a.type === value.airframeType);
  const variants = airframeByType.length > 0 ? airframeByType : airframes;

  const [icaoErrors, setIcaoErrors] = useState<{
    departure?: string;
    arrival?: string;
  }>({});

  return (
    <div className="h-full overflow-y-auto">
      {/* Flight Info */}
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
              onBlur={() =>
                setIcaoErrors((p) => ({
                  ...p,
                  departure: icaoError(value.departure),
                }))
              }
            />
          </Field>
          <Field label={t('form.arrival')} error={icaoErrors.arrival}>
            <Input
              value={value.arrival}
              placeholder={t('form.arrivalPlaceholder')}
              onChange={(e) => set({ arrival: e.target.value.toUpperCase() })}
              onBlur={() =>
                setIcaoErrors((p) => ({
                  ...p,
                  arrival: icaoError(value.arrival),
                }))
              }
            />
          </Field>
          <Field label={t('form.runway')}>
            <Input
              value={value.departureRunway}
              placeholder="—"
              onChange={(e) =>
                set({ departureRunway: e.target.value.toUpperCase() })
              }
            />
          </Field>
          <Field label={`${t('form.runway')} (Arr)`}>
            <Input
              value={value.arrivalRunway}
              placeholder="—"
              onChange={(e) =>
                set({ arrivalRunway: e.target.value.toUpperCase() })
              }
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

      {/* Optional Entries（决策 7；Phase 10 展开） */}
      <Section title={t('form.optional')} defaultOpen={false}>
        <p className="text-xs text-muted-foreground">{t('form.optionalHint')}</p>
      </Section>

      {/* Fuel Planning（决策 11/21；数值 Phase 10 填充） */}
      <Section title={t('form.fuel')} defaultOpen={false}>
        <p className="text-xs text-muted-foreground">{t('form.fuelHint')}</p>
      </Section>

      {/* Route */}
      <Section title={t('form.route')}>
        <Field label={t('form.routeString')}>
          <Input
            className="font-mono"
            value={value.routeString}
            placeholder={t('form.routePlaceholder')}
            onChange={(e) => set({ routeString: e.target.value.toUpperCase() })}
          />
        </Field>
        <Button
          variant="outline"
          size="sm"
          type="button"
          onClick={() => {
            const html = window.prompt('粘贴 SimBrief OFP 页面 HTML 或 URL 内容');
            if (html) onImportSimBrief(html);
          }}
        >
          <Import className="h-3.5 w-3.5" aria-hidden="true" />
          {t('form.importSimBrief')}
        </Button>
      </Section>

      <div className="p-3">
        <Button
          className="w-full"
          onClick={onGenerateCandidates}
          disabled={loading || !value.departure.trim() || !value.arrival.trim()}
        >
          {loading ? t('form.generating') : t('form.generateCandidates')}
        </Button>
      </div>
    </div>
  );
}
