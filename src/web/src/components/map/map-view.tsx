// 地图与图层系统（决策 33/40/41：Leaflet 底图随主题换源；LayerManager
// 显式开关 + localStorage 持久化；候选全部同色 + 选中高亮；点击航路点信息卡）。
// 审查修复：候选形状顶层 points；主题切换只换 TileLayer 源（不再重挂载）；
// 候选变化 fitBounds；polyline key 稳定；React.memo 防无关重渲染。

import 'leaflet/dist/leaflet.css';

import { Layers } from 'lucide-react';
import { memo, useEffect, useMemo, useState } from 'react';
import { useTranslation } from 'react-i18next';
import {
  CircleMarker,
  MapContainer,
  Polyline,
  Popup,
  TileLayer,
  Tooltip,
  useMap,
} from 'react-leaflet';

import type { Alternate, RouteCandidate } from '../../api/types';
import { useTheme, type Theme } from '../theme-provider';
import { Switch } from '../ui/switch';

const BASEMAPS: Record<'light' | 'dark', string> = {
  light:
    'https://{s}.basemaps.cartocdn.com/rastertiles/voyager/{z}/{x}/{y}{r}.png',
  dark: 'https://{s}.basemaps.cartocdn.com/rastertiles/dark_all/{z}/{x}/{y}{r}.png',
};

const LAYER_KEYS = ['basemap', 'routes', 'waypoints', 'alternates'] as const;
export type LayerKey = (typeof LAYER_KEYS)[number];
const STORAGE_KEY = 'px-map-layers';

/** 图层显隐（localStorage 持久化，契约：层间不互知仅经 LayerManager 注册）。 */
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

/** 图层面板（SimBrief 式：SVG 图标 + 显式开关）。 */
function LayerPanel({
  visible,
  onToggle,
}: {
  visible: Set<LayerKey>;
  onToggle: (key: LayerKey, on: boolean) => void;
}) {
  const { t } = useTranslation();
  const labels: Record<LayerKey, string> = {
    basemap: 'Basemap',
    routes: t('candidates.title'),
    waypoints: 'Waypoints',
    alternates: 'Alternates',
  };
  return (
    <div className="absolute right-2 top-2 z-[1000] w-44 rounded-md border border-border bg-card p-2 shadow-md">
      <div className="mb-1 flex items-center gap-1.5 text-xs font-semibold text-muted-foreground">
        <Layers className="h-3.5 w-3.5" aria-hidden="true" />
        Layers
      </div>
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

/** 候选加载/变化 → fitBounds（center/zoom 挂载后不生效——审查修复）。 */
function FitBounds({ candidates }: { candidates: RouteCandidate[] }) {
  const map = useMap();
  const points = useMemo(() => allPoints(candidates), [candidates]);
  useEffect(() => {
    if (points.length === 0) return;
    map.fitBounds(
      points.map((p) => [p.lat, p.lon] as [number, number]),
      { padding: [24, 24] },
    );
  }, [map, points]);
  return null;
}

export interface MapViewProps {
  candidates: RouteCandidate[];
  selected: RouteCandidate | null;
  alternates: Alternate[];
  theme: Theme;
}

export const MapView = memo(function MapView({
  candidates,
  selected,
  alternates,
  theme,
}: MapViewProps) {
  const [visible, toggle] = useLayerVisibility();
  // 审查修复：system 主题解析统一走 provider（本地 matchMedia 快照在
  // OS 切换时不更新）。
  const { resolvedTheme } = useTheme();
  void theme;
  const center = centerOf(candidates);
  const zoom = candidates.length > 0 ? 6 : 4;

  // 稳定引用（防止无关 re-render 触发 react-leaflet 全量更新）。
  const selectedIndex = selected?.index ?? null;
  const routeLines = useMemo(
    () =>
      candidates.map((c) => ({
        index: c.index,
        points: c.points.map((p) => [p.lat, p.lon] as [number, number]),
        isSelected: c.index === selectedIndex,
      })),
    [candidates, selectedIndex],
  );

  return (
    <div className="relative h-full w-full">
      <MapContainer
        center={center}
        zoom={zoom}
        className="h-full w-full"
        zoomControl={false}
        attributionControl={true}
      >
        {visible.has('basemap') && (
          <TileLayer
            url={BASEMAPS[resolvedTheme]}
            attribution='&copy; <a href="https://carto.com/">CARTO</a>'
          />
        )}
        {candidates.length > 0 && <FitBounds candidates={candidates} />}
        {/* 候选航路层（决策 42：全部同色细线 + 选中高亮；key 稳定防重挂载） */}
        {visible.has('routes') &&
          routeLines.map((r) => (
            <Polyline
              key={r.index}
              positions={r.points}
              pathOptions={{
                color: r.isSelected ? '#2563EB' : '#64748B',
                weight: r.isSelected ? 4 : 1.5,
                opacity: r.isSelected ? 1 : 0.5,
              }}
            />
          ))}
        {/* 航路点层（决策 41：Phase 9 全部同色；点击信息卡） */}
        {visible.has('waypoints') &&
          candidates.map((c) =>
            c.points.map((p) => (
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
                <Tooltip>{p.ident}</Tooltip>
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
            )),
          )}
        {/* 备降机场层（lat/lon 来自服务端，审查修复） */}
        {visible.has('alternates') &&
          alternates.map((a) => (
            <CircleMarker
              key={a.icao}
              center={[a.lat, a.lon]}
              radius={5}
              pathOptions={{
                color: '#D97706',
                fillColor: '#D97706',
                fillOpacity: 0.6,
              }}
            >
              <Tooltip>{`${a.icao} · ${Math.round(a.distance_nm)}NM`}</Tooltip>
            </CircleMarker>
          ))}
      </MapContainer>
      <LayerPanel visible={visible} onToggle={toggle} />
    </div>
  );
});
