// 飞行计划页（决策 34/42 + S9.1 D54/D56）：两阶段流程（表单 → Task）。
// Route 分区 plan.analyze 校验 + 有效性反馈；分析成功 → 规划航路在地图
// 高亮（主色粗线 + 永久点标签）；底部生成按钮 enabled iff 规划航路存在；
// 返回按钮仅表单 ↔ Task 切换。加载/错误反馈（review 修订 G1/G3）。

import { useTranslation } from 'react-i18next';
import { useEffect, useMemo, useState } from 'react';
import { ArrowLeft, Import, PenLine, TriangleAlert } from 'lucide-react';

import { callRpc, RpcError } from '../lib/rpc';
import { rpcErrorKey } from '../lib/errors';
import { isSameRoute } from '../lib/route-match';
import type { SimBriefImport } from '../lib/simbrief-import';
import {
  candidatesFingerprint,
  INITIAL_FORM,
  type PlanFormState,
} from '../components/flightplan/form-state';
import { PlanForm } from '../components/flightplan/plan-form';
import { SimBriefTab } from '../components/flightplan/simbrief-tab';
import {
  FlightTask,
  type WarningKey,
} from '../components/flightplan/flight-task';
import { MapView, type PlannedRoute } from '../components/map/map-view';
import { useTheme } from '../components/theme-provider';
import { Button } from '../components/ui/button';
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogHeader,
  DialogTitle,
} from '../components/ui/dialog';
import { cn } from '../lib/utils';
import type {
  Airframe,
  Alternate,
  AnalyzeResult,
  FlightPlan,
  GenerateParams,
  ListCyclesResult,
  PlanExportResult,
  Profile,
  RouteCandidate,
  RoutesParams,
} from '../api/types';

type Stage = 'form' | 'task';
type Tab = 'local' | 'simbrief';

function toOptionalNumber(s: string): number | undefined {
  const n = Number(s);
  return Number.isFinite(n) && s.trim() !== '' ? n : undefined;
}

export function FlightPlanPage() {
  const { t } = useTranslation();
  const { theme } = useTheme();

  const [form, setForm] = useState<PlanFormState>(INITIAL_FORM);
  const [stage, setStage] = useState<Stage>('form');
  const [tab, setTab] = useState<Tab>('local');
  const [airframes, setAirframes] = useState<Airframe[]>([]);
  const [alternates, setAlternates] = useState<Alternate[]>([]);
  const [cycles, setCycles] = useState<number[]>([]);
  const [analyzed, setAnalyzed] = useState<AnalyzeResult | null>(null);
  const [plannedRoute, setPlannedRoute] = useState<PlannedRoute | null>(null);
  // Suggest Route（D55 rev.）。
  const [profiles, setProfiles] = useState<Profile[]>([]);
  const [selectedProfile, setSelectedProfile] = useState('');
  const [candidates, setCandidates] = useState<RouteCandidate[]>([]);
  const [hoveredIndex, setHoveredIndex] = useState<number | null>(null);
  const [replaceTarget, setReplaceTarget] = useState<RouteCandidate | null>(
    null,
  );
  const [plan, setPlan] = useState<FlightPlan | null>(null);
  const [generatedFingerprint, setGeneratedFingerprint] = useState('');
  const [dismissed, setDismissed] = useState<WarningKey[]>([]);
  const [loading, setLoading] = useState<
    'analyze' | 'routes' | 'generate' | 'export' | null
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

  // 挂载：airframe 档案 + AIRAC 周期 + 偏好档案（决策 6 + D55；取消守卫
  // ——审查修复：卸载/StrictMode 双挂载后响应写过期状态）。
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
    callRpc<Profile[]>('profile.list', {})
      .then((r) => {
        if (!cancelled) setProfiles(r);
      })
      .catch(() => setProfiles([]));
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

  const currentAirframe =
    airframes.find(
      (a) => a.type === form.airframeType && a.variant === form.airframeVariant,
    ) ?? null;

  // type 唯一匹配 → 自动选其 variant（审查修复：两级选择断链——导入只带
  // 机型；页面/导入链共用同一派生，防重复逻辑漂移）。
  const autoVariantOf = (type: string): string => {
    const candidates = airframes.filter((a) => a.type === type);
    return candidates.length === 1 ? candidates[0].variant : '';
  };

  // 表单变更：Route 输入编辑 → 旧分析/规划航路失效（评审修复：生成门
  // 禁必须跟随当前输入，不得沿用陈旧航路）。
  const onFormChange = (next: PlanFormState) => {
    if (next.routeString !== form.routeString) {
      setAnalyzed(null);
      setPlannedRoute(null);
    }
    setForm(next);
  };

  // Route 分析（D54：plan.analyze；成功 → 规划航路 + 地图高亮）。
  const onAnalyzeRoute = async () => {
    setError(null);
    setLoading('analyze');
    try {
      const result = await callRpc<AnalyzeResult>('plan.analyze', {
        route_string: form.routeString,
      });
      setAnalyzed(result);
      if (result.valid) {
        setPlannedRoute({
          route_string: form.routeString,
          points: result.points ?? [],
          distance_nm: result.distance_nm,
        });
      } else {
        // 分析失败 → 旧规划航路失效（信息框显示逐条错误）。
        setPlannedRoute(null);
      }
    } catch (e) {
      setError(describeError(e));
    } finally {
      setLoading(null);
    }
  };

  // Suggest 候选生成（D55 rev.：偏好档案参数合并 → plan.routes k=5）。
  const onGenerateCandidates = async () => {
    setError(null);
    setLoading('routes');
    try {
      const profile = profiles.find((p) => p.name === selectedProfile);
      const params: RoutesParams = {
        departure: form.departure.trim().toUpperCase(),
        arrival: form.arrival.trim().toUpperCase(),
        k: profile?.k ?? 5,
        min_fl: profile?.min_fl ?? toOptionalNumber(form.minFl),
        max_fl: profile?.max_fl ?? toOptionalNumber(form.maxFl),
        level: profile?.level,
        avoid_waypoints: profile?.avoid_waypoints,
      };
      const result = await callRpc<RouteCandidate[]>('plan.routes', params);
      setCandidates(result);
      setHoveredIndex(null);
    } catch (e) {
      setError(describeError(e));
    } finally {
      setLoading(null);
    }
  };

  // 偏好档案切换 → 清空候选（D55 rev.：过期参数语义）。
  const onProfileChange = (name: string) => {
    setSelectedProfile(name);
    setCandidates([]);
    setHoveredIndex(null);
  };

  // 候选落地（D55 rev.：候选自带完整点 → 直接成为规划航路，免重分析）。
  const applyCandidate = (c: RouteCandidate) => {
    setForm((prev) => ({ ...prev, routeString: c.route_string }));
    setPlannedRoute({
      route_string: c.route_string,
      points: c.points,
      distance_nm: c.total_distance_nm,
      fromCandidate: true,
    });
    setAnalyzed(null); // 分析反馈清除（候选即规划航路）。
  };

  // 点选候选：无规划航路直接填；已有且不同 → 替换确认 Dialog。
  const onPickCandidate = (c: RouteCandidate) => {
    if (plannedRoute && !isSameRoute(plannedRoute.route_string, c.route_string)) {
      setReplaceTarget(c);
    } else {
      applyCandidate(c);
    }
  };

  const onConfirmReplace = () => {
    if (replaceTarget) applyCandidate(replaceTarget);
    setReplaceTarget(null);
  };

  // 生成计划（D56 两阶段：route_string = 规划航路）。
  const onGeneratePlan = async () => {
    if (!currentAirframe) {
      setError(t('form.type') + ' — ' + t('errors.badRequest'));
      return;
    }
    if (!plannedRoute) return;
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
        route_string: plannedRoute.route_string,
        airframe: currentAirframe,
        callsign: form.callsign || undefined,
        etd: form.etd || undefined,
        alternate: form.alternate || undefined,
        min_fl: toOptionalNumber(form.minFl),
        max_fl: toOptionalNumber(form.maxFl),
        altitude_rule: form.altitudeRule,
        // 审查修复：仅候选替换路径标记 mora_checked——分析/手写航路
        // 不得谎报地形安全（决策 26 语义）。
        candidate: plannedRoute.fromCandidate === true,
        ...(form.payloadMode === 'zfw'
          ? { zfw_kg: toOptionalNumber(form.zfwKg) }
          : {
              pax_count: Math.max(0, Number(form.paxCount) || 0),
              cargo_kg: toOptionalNumber(form.cargoKg),
            }),
      };
      const result = await callRpc<FlightPlan>('plan.generate', params);
      setPlan(result);
      setGeneratedFingerprint(candidatesFingerprint(form));
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

  // SimBrief 导入链（D52：解析 → 表单回填 → analyze → routes[0] →
  // generate(candidate) → 直达 Task）。
  const onSimBriefImported = async (parsed: SimBriefImport) => {
    setForm((prev) => {
      const type = parsed.airframeType || prev.airframeType;
      return {
        ...prev,
        departure: parsed.departure || prev.departure,
        arrival: parsed.arrival || prev.arrival,
        routeString: parsed.routeString || prev.routeString,
        airframeType: type,
        // type 唯一匹配 → 自动选其 variant（否则留待用户选）。
        airframeVariant: autoVariantOf(type) || prev.airframeVariant,
      };
    });
    // 回填后立即取最新表单值执行链。
    const filled = {
      ...INITIAL_FORM,
      departure: parsed.departure,
      arrival: parsed.arrival,
      routeString: parsed.routeString,
      airframeType: parsed.airframeType,
      airframeVariant: autoVariantOf(parsed.airframeType),
    };
    setError(null);
    setLoading('generate');
    try {
      // 1) 校验导入航路。
      const analyze = await callRpc<AnalyzeResult>('plan.analyze', {
        route_string: parsed.routeString,
      });
      if (!analyze.valid) {
        setError(
          analyze.errors?.map((e) => e.message).join('；') ??
            t('errors.parseError'),
        );
        return;
      }
      // 2) 取第一个候选（MORA 已校验 → candidate 标记合法）。
      // 机型：与回填一致的派生——type 唯一匹配时变体已自动选入 filled。
      const airframe =
        airframes.find(
          (a) =>
            a.type === filled.airframeType &&
            a.variant === filled.airframeVariant,
        ) ?? null;
      const routes = await callRpc<RouteCandidate[]>('plan.routes', {
        departure: parsed.departure,
        arrival: parsed.arrival,
        k: 5,
        min_fl: toOptionalNumber(filled.minFl),
        max_fl: toOptionalNumber(filled.maxFl),
      });
      const first = routes[0];
      if (!first) {
        setError(t('errors.noSolution'));
        return;
      }
      if (!airframe) {
        setError(t('form.type') + ' — ' + t('errors.badRequest'));
        return;
      }
      // 3) 生成（显式 pax/cargo 0；候选 → 地形安全已校验）。
      const params: GenerateParams = {
        route_string: first.route_string,
        airframe,
        pax_count: 0,
        cargo_kg: 0,
        candidate: true,
      };
      const result = await callRpc<FlightPlan>('plan.generate', params);
      setPlan(result);
      setGeneratedFingerprint(candidatesFingerprint(filled));
      setStage('task');
      setTab('local');
    } catch (e) {
      setError(describeError(e));
    } finally {
      setLoading(null);
    }
  };

  // 过期检测缓存（每按键重算 JSON.stringify——审查修复；D55 rev.：
  // fingerprint 不含 route_string）。
  const stale = useMemo(
    () =>
      stage !== 'form' &&
      candidatesFingerprint(form) !== generatedFingerprint,
    [stage, form, generatedFingerprint],
  );

  return (
    <div className="relative flex h-full min-h-0 flex-col lg:flex-row">
      {/* 地图背景层（S9.1 D50：absolute inset-0 覆盖全容器；任务面板玻璃浮层） */}
      <MapView
        candidates={candidates}
        hoveredCandidateIndex={hoveredIndex}
        alternates={alternates}
        theme={theme}
        plannedRoute={plannedRoute}
      />
      {/* 地图槽（T3 验收：窄窗 min-h-[240px]；flex 强制面板 ≤55% + 地图 ≥240px） */}
      <div className="relative min-h-[240px] flex-1 lg:min-h-0" aria-hidden />
      <section className="relative z-10 flex w-full max-h-[55%] shrink-0 flex-col overflow-y-auto border-b border-border bg-background/75 backdrop-blur-md lg:max-h-none lg:w-96 lg:border-b-0 lg:border-r">
        {/* 双标签（D52：本地表单 / SimBrief 导入；切换保留表单状态） */}
        {stage === 'form' && (
          <div
            role="tablist"
            aria-label={t('tabs.local')}
            className="flex shrink-0 border-b border-border"
          >
            {(
              [
                { key: 'local', icon: PenLine, label: t('tabs.local') },
                { key: 'simbrief', icon: Import, label: t('tabs.simbrief') },
              ] as const
            ).map(({ key, icon: Icon, label }) => (
              <button
                key={key}
                type="button"
                role="tab"
                aria-selected={tab === key}
                onClick={() => setTab(key)}
                className={cn(
                  'flex flex-1 items-center justify-center gap-1.5 px-3 py-2 text-sm font-medium text-muted-foreground hover:text-foreground',
                  tab === key &&
                    'border-b-2 border-secondary text-foreground',
                )}
              >
                <Icon className="h-3.5 w-3.5" aria-hidden="true" />
                {label}
              </button>
            ))}
          </div>
        )}
        {stage === 'task' && (
          <div className="flex items-center gap-2 border-b border-border px-2 py-1.5">
            <Button
              variant="ghost"
              size="sm"
              onClick={() => setStage('form')}
            >
              <ArrowLeft className="h-3.5 w-3.5" aria-hidden="true" />
              {t('stage.back')}
            </Button>
            <span className="text-sm font-semibold">{t('stage.task')}</span>
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
          {stage === 'form' && tab === 'local' && (
            <div className="flex min-h-full flex-col">
              <PlanForm
                value={form}
                onChange={onFormChange}
                airframes={airframes}
                alternates={alternates}
                onAnalyzeRoute={onAnalyzeRoute}
                analyzing={loading === 'analyze'}
                analyzed={analyzed}
                onGeneratePlan={onGeneratePlan}
                generating={loading === 'generate'}
                canGenerate={plannedRoute !== null}
                profiles={profiles}
                selectedProfile={selectedProfile}
                onProfileChange={onProfileChange}
                candidates={candidates}
                onGenerateCandidates={onGenerateCandidates}
                generatingCandidates={loading === 'routes'}
                hoveredCandidateIndex={hoveredIndex}
                onHoverCandidate={setHoveredIndex}
                onPickCandidate={onPickCandidate}
              />
            </div>
          )}
          {stage === 'form' && tab === 'simbrief' && (
            <SimBriefTab
              onImported={onSimBriefImported}
              busy={loading !== null}
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
              distanceNm={plannedRoute?.distance_nm}
            />
          )}
        </div>
      </section>
      {/* 候选替换确认（D55 rev.：已有规划航路且与候选不同） */}
      <Dialog
        open={replaceTarget !== null}
        onOpenChange={(o) => !o && setReplaceTarget(null)}
      >
        <DialogContent>
          <DialogHeader>
            <DialogTitle>{t('suggest.replaceTitle')}</DialogTitle>
            <DialogDescription>{t('suggest.replaceDesc')}</DialogDescription>
          </DialogHeader>
          <div className="flex justify-end gap-2">
            <Button
              variant="ghost"
              size="sm"
              onClick={() => setReplaceTarget(null)}
            >
              {t('suggest.cancel')}
            </Button>
            <Button size="sm" onClick={onConfirmReplace}>
              {t('suggest.confirm')}
            </Button>
          </div>
        </DialogContent>
      </Dialog>
    </div>
  );
}
