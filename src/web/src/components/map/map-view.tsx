// 地图与图层系统（决策 33/40/41 + S9.1 D51/D58/D59）：底图常显不设开关；
// 图层开关收进右上圆角工具栏列（候选航路/航路点/备降机场）；主题切换
// 同列（D58）。标签分层（D59）：机场级永久 Tooltip 12px（zoom≥5）、
// 规划航路点 10px（zoom≥7，T4 接入）、候选点悬停显示（T6 接入）。
// 审查修复：候选形状顶层 points；主题切换只换 TileLayer 源（不再重挂载）；
// 候选变化 fitBounds；polyline key 稳定；React.memo 防无关重渲染。

import 'leaflet/dist/leaflet.css';

import L from 'leaflet';
import { Layers } from 'lucide-react';
import { memo, useEffect, useMemo, useState } from 'react';
import { useTranslation } from 'react-i18next';
import {
  CircleMarker,
  MapContainer,
  Marker,
  Polyline,
  Popup,
  TileLayer,
  Tooltip,
  useMap,
} from 'react-leaflet';

import type { Alternate, RouteCandidate, RoutePoint } from '../../api/types';
import { useTheme, type Theme } from '../theme-provider';
import { ThemeToggle } from '../theme-toggle';

/** 规划航路（S9.1 D54：分析成功或候选替换后存在；地图主色高亮）。 */
export interface PlannedRoute {
  route_string: string;
  points: RoutePoint[];
  distance_nm?: number;
  /** 来自候选替换（MORA 已校验）——generate 带 candidate 标记（决策 26）。 */
  fromCandidate?: boolean;
}
import { Button } from '../ui/button';
import { Switch } from '../ui/switch';
import {
  Tooltip as UITooltip,
  TooltipContent,
  TooltipTrigger,
} from '../ui/tooltip';

const BASEMAPS: Record<'light' | 'dark', string> = {
  light:
    'https://{s}.basemaps.cartocdn.com/rastertiles/voyager/{z}/{x}/{y}{r}.png',
  dark: 'https://{s}.basemaps.cartocdn.com/rastertiles/dark_all/{z}/{x}/{y}{r}.png',
};

// S9.1 D51：底图常显——开关只留数据图层（旧 localStorage 中的 'basemap'
// 键被 includes 过滤，自动失效）。
const LAYER_KEYS = ['routes', 'waypoints', 'alternates'] as const;
export type LayerKey = (typeof LAYER_KEYS)[number];
const STORAGE_KEY = 'px-map-layers';

/** 图层显隐（localStorage 持久化，契约：层间不互知仅经 ToolbarColumn 注册）。 */
export function useLayerVisibility(): [
  Set<LayerKey>,
  (key: LayerKey, on: boolean) => void,
] {
  const [visible, setVisible] = useState<Set<LayerKey>>(() => {
    try {
      const stored = JSON.parse(localStorage.getItem(STORAGE_KEY) ?? 'null');
      if (Array.isArray(stored) && stored.length > 0) {
        return new Set(stored.filter((k) => LAYER_KEYS.includes(k)));
      }
    } catch {
      // 存储损坏回退默认。
    }
    return new Set(LAYER_KEYS);
  });
  const toggle = (key: LayerKey, on: boolean) => {
    setVisible((prev) => {
      const next = new Set(prev);
      if (on) next.add(key);
      else next.delete(key);
      return next;
    });
  };

  // 持久化独立 effect（审查修复：setItem 在 updater 内 StrictMode 双跑）。
  useEffect(() => {
    localStorage.setItem(STORAGE_KEY, JSON.stringify([...visible]));
  }, [visible]);
  return [visible, toggle];
}

/** 地图缩放跟踪（D59：zoomend → zoomLevel，驱动标签分层渲染）。 */
function ZoomTracker({ onZoom }: { onZoom: (zoom: number) => void }) {
  const map = useMap();
  useEffect(() => {
    const listener = () => onZoom(map.getZoom());
    map.on('zoomend', listener);
    onZoom(map.getZoom());
    return () => {
      map.off('zoomend', listener);
    };
  }, [map, onZoom]);
  return null;
}

/** 右上圆角工具栏列（D58/D51）：Layers 下拉 + 主题切换；窄窗锚定地图区。 */
function ToolbarColumn({
  visible,
  onToggle,
}: {
  visible: Set<LayerKey>;
  onToggle: (key: LayerKey, on: boolean) => void;
}) {
  const { t } = useTranslation();
  const [open, setOpen] = useState(false);
  const labels: Record<LayerKey, string> = {
    routes: t('layers.routes'),
    waypoints: t('layers.waypoints'),
    alternates: t('layers.alternates'),
  };
  return (
    <div className="absolute right-2 top-[55%] z-[1000] lg:top-2">
      <div className="flex flex-col items-center gap-0.5 rounded-xl border border-border bg-card/80 p-1 shadow-md backdrop-blur">
        <UITooltip>
          <TooltipTrigger asChild>
            <Button
              variant="ghost"
              size="icon"
              aria-label={t('layers.title')}
              aria-expanded={open}
              onClick={() => setOpen((v) => !v)}
              className="h-8 w-8"
            >
              <Layers className="h-4 w-4" aria-hidden="true" />
            </Button>
          </TooltipTrigger>
          <TooltipContent>{t('layers.title')}</TooltipContent>
        </UITooltip>
        <ThemeToggle />
      </div>
      {open && (
        <div className="absolute right-0 top-full mt-1 w-40 rounded-md border border-border bg-card p-2 shadow-md">
          {LAYER_KEYS.map((key) => (
            <label
              key={key}
              className="flex cursor-pointer items-center justify-between py-0.5 text-xs"
            >
              {labels[key]}
              <Switch
                checked={visible.has(key)}
                onCheckedChange={(on) => onToggle(key, on)}
                aria-label={labels[key]}
              />
            </label>
          ))}
        </div>
      )}
    </div>
  );
}

/** 候选点集（形状适配：顶层 points）。 */
function allPoints(candidates: RouteCandidate[]): Array<{ lat: number; lon: number }> {
  return candidates.flatMap((c) => c.points);
}

/** 地图中心：起降场中点（有数据时），否则中国区域。 */
function centerOf(candidates: RouteCandidate[]): [number, number] {
  const points = allPoints(candidates);
  if (points.length === 0) return [35, 105];
  let lat = 0;
  let lon = 0;
  for (const p of points) {
    lat += p.lat;
    lon += p.lon;
  }
  return [lat / points.length, lon / points.length];
}

/** 候选/规划航路加载变化 → fitBounds（center/zoom 挂载后不生效——审查修复）。
    地图居中修正（3:7 分屏）：宽窗下左侧 30% 被任务面板覆盖，可视中心
    应为剩余 70% 区域的中点——用 paddingTopLeft 把内容适配进右侧可视区；
    窄窗面板在上方，无左遮挡。 */
function FitBounds({ points }: { points: Array<{ lat: number; lon: number }> }) {
  const map = useMap();
  const coords = useMemo(
    () => points.map((p) => [p.lat, p.lon] as [number, number]),
    [points],
  );
  useEffect(() => {
    if (coords.length === 0) return;
    const size = map.getSize();
    // 面板宽 = max(30% 容器, 320px min-w) + rail 56px；lg 以下无遮挡。
    const panelWidth =
      window.matchMedia('(min-width: 1024px)').matches
        ? Math.max(size.x * 0.3, 320) + 56
        : 0;
    map.fitBounds(coords, {
      padding: [24, 24],
      paddingTopLeft: [panelWidth, 0],
    });
  }, [map, coords]);
  return null;
}

export interface MapViewProps {
  candidates: RouteCandidate[];
  /** 悬停候选（D55 rev.：高亮 + 临时点标签）。 */
  hoveredCandidateIndex: number | null;
  alternates: Alternate[];
  theme: Theme;
  /** 规划航路（S9.1 D54：主色粗线 + 永久分层标签）。 */
  plannedRoute?: PlannedRoute | null;
  /** 备降菜单 hover/选中联动：聚焦的备降场 ICAO（grill 确认——仅显示该
      场 + 到达场橙色虚线；NONE/null = 显示全部）。 */
  alternateFocus?: string | null;
}

export const MapView = memo(function MapView({
  candidates,
  hoveredCandidateIndex,
  alternates,
  theme,
  plannedRoute,
  alternateFocus,
}: MapViewProps) {
  const [visible, toggle] = useLayerVisibility();
  // 审查修复：system 主题解析统一走 provider（本地 matchMedia 快照在
  // OS 切换时不更新）。
  const { resolvedTheme } = useTheme();
  void theme;
  const center = centerOf(candidates);
  const zoom = candidates.length > 0 ? 6 : 4;
  const [zoomLevel, setZoomLevel] = useState(zoom);

  // 稳定引用（防止无关 re-render 触发 react-leaflet 全量更新）。
  const routeLines = useMemo(
    () =>
      candidates.map((c) => ({
        index: c.index,
        points: c.points.map((p) => [p.lat, p.lon] as [number, number]),
        isHovered: c.index === hoveredCandidateIndex,
      })),
    [candidates, hoveredCandidateIndex],
  );

  // fitBounds 输入：候选点 + 规划航路点（D54：分析后聚焦规划航路）。
  const fitPoints = useMemo(
    () => [...allPoints(candidates), ...(plannedRoute?.points ?? [])],
    [candidates, plannedRoute],
  );

  // D59：标签分层——机场级 12px（zoom≥5）常显；规划航路点 10px
  // （zoom≥7）常显；候选点仅悬停（T6 接入）。备降方块阈值更高（zoom≥7，
  // 用户要求备降 zoom 更大——更放大才显示）。
  const showAirportLabels = zoomLevel >= 5;
  const showPlannedPointLabels = zoomLevel >= 7;
  const showAlternates = zoomLevel >= 7;
  // 备降聚焦：到达场 = 规划航路末点（segment_index -1）；聚焦时只显示
  // 该备降场 + 到达场 ↔ 备降场橙色虚线。
  const arrivalPoint =
    plannedRoute?.points.find((p) => p.segment_index === -1) ?? null;
  const focused = alternateFocus ?? null;
  const focusedAlternate = focused
    ? alternates.find((a) => a.icao === focused) ?? null
    : null;

  return (
    <div className="absolute inset-0">
      <MapContainer
        center={center}
        zoom={zoom}
        className="h-full w-full"
        zoomControl={false}
        attributionControl={true}
        // 循环世界（用户反馈）：minZoom 防缩小成条带；worldCopyJump = 左右
        // 滑动跨越世界边界时自动跳回主世界——瓦片无限循环、矢量（航路/
        // 备降）始终在主世界渲染，不会出现消失或重复条带。
        minZoom={3}
        worldCopyJump
      >
        <ZoomTracker onZoom={setZoomLevel} />
        {/* 底图常显（D51：无死态）；瓦片随世界循环（配合 worldCopyJump）。
            跨边界跳变防刷新感：keepBuffer 预取更广、fade 关闭（缓存命中
            直接替换）、拖动中持续更新（避免停止瞬间集中加载）。 */}
        <TileLayer
          url={BASEMAPS[resolvedTheme]}
          keepBuffer={4}
          updateWhenIdle={false}
          attribution='&copy; <a href="https://carto.com/">CARTO</a>'
        />
        {fitPoints.length > 0 && <FitBounds points={fitPoints} />}
        {/* 候选航路层（D55 rev.：全部常灰细线；悬停卡高亮——
            与规划航路主色区分；key 稳定防重挂载） */}
        {visible.has('routes') &&
          routeLines.map((r) => (
            <Polyline
              key={r.index}
              positions={r.points}
              pathOptions={{
                color: r.isHovered ? '#94A3B8' : '#64748B',
                weight: r.isHovered ? 3 : 1.5,
                opacity: r.isHovered ? 1 : 0.5,
              }}
            />
          ))}
        {/* 航路点层（D59：端点机场标签常显 12px zoom≥5；中间点悬停候选
            临时显示；点击信息卡——Tooltip 互斥渲染防 bindTooltip 覆盖） */}
        {visible.has('waypoints') &&
          candidates.map((c) =>
            c.points.map((p) => {
              const isAirport = p.segment_index === -1;
              const hovered = hoveredCandidateIndex === c.index;
              const showPermanent =
                hovered || (isAirport && showAirportLabels);
              return (
                <CircleMarker
                  key={`${c.index}-${p.segment_index}-${p.ident}`}
                  center={[p.lat, p.lon]}
                  radius={3}
                  pathOptions={{
                    color: '#059669',
                    fillColor: '#059669',
                    fillOpacity: 0.9,
                  }}
                >
                  {showPermanent ? (
                    <Tooltip
                      permanent
                      direction="top"
                      interactive={false}
                      className={
                        isAirport
                          ? 'px-label px-label-airport'
                          : 'px-label px-label-waypoint'
                      }
                    >
                      {p.ident}
                    </Tooltip>
                  ) : (
                    <Tooltip>{p.ident}</Tooltip>
                  )}
                  <Popup>
                    <div className="space-y-0.5 font-mono text-xs">
                      <div className="font-semibold">{p.ident}</div>
                      {p.via && <div>via {p.via}</div>}
                      <div>
                        {p.lat.toFixed(4)}, {p.lon.toFixed(4)}
                      </div>
                      <div className="text-muted-foreground">--</div>
                    </div>
                  </Popup>
                </CircleMarker>
              );
            }),
          )}
        {/* 备降机场层（橙色方块，zoom≥7 显示；菜单 hover/选中聚焦时只
            显示聚焦场，NONE/null 显示全部；到达场 ↔ 聚焦场橙色虚线） */}
        {visible.has('alternates') &&
          showAlternates &&
          (focusedAlternate
            ? [
                <Polyline
                  key="alt-dash"
                  positions={
                    arrivalPoint
                      ? ([
                          [arrivalPoint.lat, arrivalPoint.lon],
                          [focusedAlternate.lat, focusedAlternate.lon],
                        ] as [number, number][])
                      : ([] as [number, number][])
                  }
                  pathOptions={{
                    color: '#D97706',
                    weight: 2,
                    opacity: 0.6,
                    dashArray: '6 6',
                  }}
                />,
                <Marker
                  key={focusedAlternate.icao}
                  position={[focusedAlternate.lat, focusedAlternate.lon]}
                  icon={L.divIcon({
                    className: 'px-waypoint-box px-waypoint-box-alt',
                    html: `<span class="px-waypoint-tag px-waypoint-tag-airport">${focusedAlternate.icao} · ${Math.round(
                      focusedAlternate.distance_nm,
                    )}NM</span>`,
                    iconSize: [14, 14],
                    iconAnchor: [7, 7],
                  })}
                />,
              ]
            : alternates.map((a) => (
                <Marker
                  key={a.icao}
                  position={[a.lat, a.lon]}
                  icon={L.divIcon({
                    className: 'px-waypoint-box px-waypoint-box-alt',
                    html: `<span class="px-waypoint-tag px-waypoint-tag-airport">${a.icao} · ${Math.round(
                      a.distance_nm,
                    )}NM</span>`,
                    iconSize: [14, 14],
                    iconAnchor: [7, 7],
                  })}
                />
              )))}
        {/* 规划航路层（热修复：airway 段蓝色粗线 #2563EB；航路点绿色
            方块标识——仅 Route 分析航路生效，suggest 候选保持圆点；
            方块 zoom 分级显示：机场 zoom≥5 / 航路点 zoom≥7，低倍率
            不渲染避免挤占） */}
        {plannedRoute && plannedRoute.points.length > 0 && (
          <>
            <Polyline
              positions={plannedRoute.points.map(
                (p) => [p.lat, p.lon] as [number, number],
              )}
              pathOptions={{ color: '#2563EB', weight: 4, opacity: 1 }}
            />
            {plannedRoute.points.map((p, i) => {
              const isAirport = p.segment_index === -1;
              const showAtZoom = isAirport
                ? showAirportLabels
                : showPlannedPointLabels;
              if (!showAtZoom) return null; // 低倍率不渲染方块（避免挤占）
              return (
                <Marker
                  key={`planned-${i}-${p.ident}`}
                  position={[p.lat, p.lon]}
                  icon={L.divIcon({
                    className: isAirport
                      ? 'px-waypoint-box px-waypoint-box-airport'
                      : 'px-waypoint-box',
                    html: `<span class="px-waypoint-tag${
                      isAirport ? ' px-waypoint-tag-airport' : ''
                    }">${p.ident}</span>`,
                    iconSize: isAirport ? [14, 14] : [10, 10],
                    iconAnchor: isAirport ? [7, 7] : [5, 5],
                  })}
                >
                  <Popup>
                    <div className="space-y-0.5 font-mono text-xs">
                      <div className="font-semibold">{p.ident}</div>
                      {p.via && <div>via {p.via}</div>}
                      <div>
                        {p.lat.toFixed(4)}, {p.lon.toFixed(4)}
                      </div>
                    </div>
                  </Popup>
                </Marker>
              );
            })}
          </>
        )}
      </MapContainer>
      <ToolbarColumn visible={visible} onToggle={toggle} />
    </div>
  );
});
