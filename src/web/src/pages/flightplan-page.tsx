// 飞行计划页（决策 34/42：三子阶段替换 + 地图；两步式交互流）。
// 表单 → 生成候选(plan.routes) → 候选列表 + 地图 → 点选 → 生成计划
// (plan.generate) → Flight Task 面板。加载/错误反馈（review 修订 G1/G3）。

import { useTranslation } from 'react-i18next';
import { useEffect, useMemo, useState } from 'react';
import { ArrowLeft, TriangleAlert } from 'lucide-react';

import { callRpc, RpcError } from '../lib/rpc';
import { rpcErrorKey } from '../lib/errors';
import { parseSimBriefOFP } from '../lib/simbrief-import';
import {
  candidatesFingerprint,
  INITIAL_FORM,
  type PlanFormState,
} from '../components/flightplan/form-state';
import { PlanForm } from '../components/flightplan/plan-form';
import { CandidateList } from '../components/flightplan/candidate-list';
import {
  FlightTask,
  type WarningKey,
} from '../components/flightplan/flight-task';
import { MapView } from '../components/map/map-view';
import { useTheme } from '../components/theme-provider';
import { Button } from '../components/ui/button';
import type {
  Airframe,
  Alternate,
  FlightPlan,
  GenerateParams,
  ListCyclesResult,
  PlanExportResult,
  RouteCandidate,
} from '../api/types';

type Stage = 'form' | 'candidates' | 'task';

function toOptionalNumber(s: string): number | undefined {
  const n = Number(s);
  return Number.isFinite(n) && s.trim() !== '' ? n : undefined;
}

export function FlightPlanPage() {
  const { t } = useTranslation();
  const { theme } = useTheme();

  const [form, setForm] = useState<PlanFormState>(INITIAL_FORM);
  const [stage, setStage] = useState<Stage>('form');
  const [airframes, setAirframes] = useState<Airframe[]>([]);
  const [alternates, setAlternates] = useState<Alternate[]>([]);
  const [cycles, setCycles] = useState<number[]>([]);
  const [candidates, setCandidates] = useState<RouteCandidate[]>([]);
  // seed 显示（决策 42）：换一批 = seed+1 功能 Phase 10 接；当前固定 0。
  const candidateSeed = 0;
  const [candidateFingerprint, setCandidateFingerprint] = useState('');
  const [selectedIndex, setSelectedIndex] = useState<number | null>(null);
  const [plan, setPlan] = useState<FlightPlan | null>(null);
  const [dismissed, setDismissed] = useState<WarningKey[]>([]);
  const [loading, setLoading] = useState<
    'routes' | 'generate' | 'export' | null
  >(null);
  const [error, setError] = useState<string | null>(null);

  const describeError = useMemo(
    () => (e: unknown): string => {
      if (e instanceof RpcError) {
        const key = rpcErrorKey(e.code);
        const base = t(key) === key ? '' : t(key);
        return `${base}${e.message && base ? `：${e.message}` : e.message}`;
      }
      return t('errors.offline');
    },
    [t],
  );

  // 挂载：airframe 档案 + AIRAC 周期（决策 6；取消守卫——审查修复：
  // 卸载/StrictMode 双挂载后响应写过期状态）。
  useEffect(() => {
    let cancelled = false;
    callRpc<Airframe[]>('airframe.list', {})
      .then((r) => {
        if (!cancelled) setAirframes(r);
      })
      .catch(() => setAirframes([]));
    callRpc<ListCyclesResult>('list_cycles', {})
      .then((r) => {
        if (!cancelled) setCycles(r.cycles);
      })
      .catch(() => setCycles([]));
    return () => {
      cancelled = true;
    };
  }, []);

  // arrival 变化 → 拉取备降候选（决策 14 修订：plan.alternates；
  // debounce 300ms + 取消守卫——审查修复：每按键一次 HTTP POST）。
  useEffect(() => {
    const arrival = form.arrival.trim().toUpperCase();
    if (arrival.length !== 4) return;
    let cancelled = false;
    const timer = setTimeout(() => {
      callRpc<Alternate[]>('plan.alternates', { arrival })
        .then((r) => {
          if (!cancelled) setAlternates(r);
        })
        .catch(() => {});
    }, 300);
    return () => {
      cancelled = true;
      clearTimeout(timer);
    };
  }, [form.arrival]);

  // 生成候选（决策 7/9：plan.routes）。
  const onGenerateCandidates = async () => {
    setError(null);
    setLoading('routes');
    try {
      const result = await callRpc<RouteCandidate[]>('plan.routes', {
        departure: form.departure.trim().toUpperCase(),
        arrival: form.arrival.trim().toUpperCase(),
        min_fl: toOptionalNumber(form.minFl),
        max_fl: toOptionalNumber(form.maxFl),
        departure_runway: form.departureRunway || undefined,
        arrival_runway: form.arrivalRunway || undefined,
      });
      setCandidates(result);
      setSelectedIndex(result.length > 0 ? result[0].index : null);
      setCandidateFingerprint(candidatesFingerprint(form));
      setStage('candidates');
    } catch (e) {
      setError(describeError(e));
    } finally {
      setLoading(null);
    }
  };

  const selected = candidates.find((c) => c.index === selectedIndex) ?? null;
  const currentAirframe =
    airframes.find(
      (a) => a.type === form.airframeType && a.variant === form.airframeVariant,
    ) ?? null;

  // 点选候选 → route_string 入状态（决策 34）。
  const onSelectCandidate = (index: number) => {
    const c = candidates.find((x) => x.index === index);
    setSelectedIndex(index);
    if (c?.route_string) {
      setForm((prev) => ({
        ...prev,
        routeString: c.route_string as string,
      }));
    }
  };

  // 生成计划（plan.generate：候选标记 + 五字段回显 + 配载）。
  const onGeneratePlan = async () => {
    if (!currentAirframe) {
      setError(t('form.type') + ' — ' + t('errors.badRequest'));
      return;
    }
    // zfw 模式空输入提前拦截（审查修复：字段被 JSON 丢弃 → 后端报
    // 泛化"配载参数必填"且不指明缺哪个）。
    if (form.payloadMode === 'zfw' && form.zfwKg.trim() === '') {
      setError(t('form.zfwKg') + ' — ' + t('errors.badRequest'));
      return;
    }
    setError(null);
    setLoading('generate');
    try {
      const params: GenerateParams = {
        route_string: selected?.route_string ?? form.routeString,
        airframe: currentAirframe,
        callsign: form.callsign || undefined,
        etd: form.etd || undefined,
        alternate: form.alternate || undefined,
        min_fl: toOptionalNumber(form.minFl),
        max_fl: toOptionalNumber(form.maxFl),
        altitude_rule: form.altitudeRule,
        // 审查修复：仅候选路径标记 mora_checked——手写航路不得谎报
        // 地形安全（决策 26 语义）。
        candidate: selected !== null,
        random_seed: candidateSeed > 0 ? candidateSeed : undefined,
        ...(form.payloadMode === 'zfw'
          ? { zfw_kg: toOptionalNumber(form.zfwKg) }
          : {
              pax_count: Math.max(0, Number(form.paxCount) || 0),
              cargo_kg: toOptionalNumber(form.cargoKg),
            }),
      };
      const result = await callRpc<FlightPlan>('plan.generate', params);
      setPlan(result);
      setStage('task');
    } catch (e) {
      setError(describeError(e));
    } finally {
      setLoading(null);
    }
  };

  // 导出 .PLN（决策 17：plan.export → 浏览器下载；Tauri 写盘 Phase 10）。
  const onExport = async () => {
    if (!plan) return;
    setError(null);
    setLoading('export');
    try {
      const result = await callRpc<PlanExportResult>('plan.export', {
        format: 'msfs2024',
        flightplan: plan,
      });
      const blob = new Blob([result.content], { type: 'application/xml' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = result.filename;
      document.body.appendChild(a);
      a.click();
      a.remove();
      // 审查修复：同步 revoke 在 WebKit/Firefox 竞态丢失下载——延迟到
      // 事件循环之后。
      setTimeout(() => URL.revokeObjectURL(url), 1000);
    } catch (e) {
      setError(describeError(e));
    } finally {
      setLoading(null);
    }
  };

  // SimBrief 最小导入（决策 39）。
  const onImportSimBrief = (html: string) => {
    const parsed = parseSimBriefOFP(html);
    if (!parsed) {
      setError(t('errors.parseError'));
      return;
    }
    setForm((prev) => {
      const type = parsed.airframeType || prev.airframeType;
      // 审查修复：导入只带机型不带构型 → 两级选择断链；type 唯一
      // 匹配时自动选其 variant，否则留待用户选。
      const candidates = airframes.filter((a) => a.type === type);
      const variant =
        candidates.length === 1 ? candidates[0].variant : prev.airframeVariant;
      return {
        ...prev,
        departure: parsed.departure || prev.departure,
        arrival: parsed.arrival || prev.arrival,
        routeString: parsed.routeString || prev.routeString,
        airframeType: type,
        airframeVariant: variant,
      };
    });
  };

  // 过期检测缓存（每按键重算 JSON.stringify——审查修复）。
  const stale = useMemo(
    () =>
      stage !== 'form' &&
      candidatesFingerprint(form) !== candidateFingerprint,
    [stage, form, candidateFingerprint],
  );

  return (
    <div className="flex h-full min-h-0">
      <section className="flex w-96 shrink-0 flex-col border-r border-border bg-card">
        {stage !== 'form' && (
          <div className="flex items-center gap-2 border-b border-border px-2 py-1.5">
            <Button
              variant="ghost"
              size="sm"
              onClick={() =>
                setStage(stage === 'task' ? 'candidates' : 'form')
              }
            >
              <ArrowLeft className="h-3.5 w-3.5" aria-hidden="true" />
              {t('stage.back')}
            </Button>
            <span className="text-sm font-semibold">
              {stage === 'candidates'
                ? t('stage.candidates')
                : t('stage.task')}
            </span>
          </div>
        )}
        {error && (
          <div
            role="alert"
            className="flex items-center gap-2 border-b border-error/40 bg-error/10 px-3 py-1.5 text-xs text-error"
          >
            <TriangleAlert
              className="h-3.5 w-3.5 shrink-0"
              aria-hidden="true"
            />
            <span className="flex-1">{error}</span>
            <button
              type="button"
              aria-label={t('task.closeWarning')}
              onClick={() => setError(null)}
              className="opacity-70 hover:opacity-100"
            >
              ×
            </button>
          </div>
        )}
        <div className="min-h-0 flex-1">
          {stage === 'form' && (
            <PlanForm
              value={form}
              onChange={setForm}
              airframes={airframes}
              alternates={alternates}
              onGenerateCandidates={onGenerateCandidates}
              loading={loading === 'routes'}
              onImportSimBrief={onImportSimBrief}
            />
          )}
          {stage === 'candidates' && (
            <CandidateList
              candidates={candidates}
              selectedIndex={selectedIndex}
              onSelect={onSelectCandidate}
              onGeneratePlan={onGeneratePlan}
              loading={loading === 'generate'}
              seed={candidateSeed}
              stale={stale}
            />
          )}
          {stage === 'task' && plan && (
            <FlightTask
              plan={plan}
              form={form}
              airframe={currentAirframe}
              cycles={cycles}
              stale={stale}
              dismissedWarnings={dismissed}
              onDismissWarning={(k) =>
                setDismissed((prev) =>
                  prev.includes(k) ? prev : [...prev, k],
                )
              }
              onExport={onExport}
              exporting={loading === 'export'}
              distanceNm={selected?.total_distance_nm}
            />
          )}
        </div>
      </section>
      <div className="min-w-0 flex-1">
        <MapView
          candidates={candidates}
          selected={selected}
          alternates={alternates}
          theme={theme}
        />
      </div>
    </div>
  );
}
