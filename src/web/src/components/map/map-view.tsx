// 地图与图层系统（决策 33/40/41：Leaflet 底图随主题换源；LayerManager
// 显式开关 + localStorage 持久化；候选全部同色 + 选中高亮；点击航路点信息卡）。

import 'leaflet/dist/leaflet.css';

import { Layers } from 'lucide-react';
import { useTranslation } from 'react-i18next';
import { useState } from 'react';
import {
  CircleMarker,
  MapContainer,
  Polyline,
  Popup,
  TileLayer,
  Tooltip,
} from 'react-leaflet';

import type { Alternate, RouteCandidate } from '../../api/types';
import type { Theme } from '../theme-provider';
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
      localStorage.setItem(STORAGE_KEY, JSON.stringify([...next]));
      return next;
    });
  };
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

export interface MapViewProps {
  candidates: RouteCandidate[];
  selected: RouteCandidate | null;
  alternates: Alternate[];
  theme: Theme;
}

/** 地图中心：起降场中点（有数据时），否则中国区域。 */
function centerOf(candidates: RouteCandidate[]): [number, number] {
  const points = candidates.flatMap((c) => c.route.points);
  if (points.length === 0) return [35, 105];
  let lat = 0;
  let lon = 0;
  for (const p of points) {
    lat += p.lat;
    lon += p.lon;
  }
  return [lat / points.length, lon / points.length];
}

/** 备降机场坐标：plan.alternates 的 route 首点即备降场自身。 */
function alternateCoord(a: Alternate): [number, number] | null {
  const route = a.route as { points?: Array<{ lat?: number; lon?: number }> } | undefined;
  const p = route?.points?.[0];
  if (p && typeof p.lat === 'number' && typeof p.lon === 'number') {
    return [p.lat, p.lon];
  }
  return null;
}

export function MapView({
  candidates,
  selected,
  alternates,
  theme,
}: MapViewProps) {
  const [visible, toggle] = useLayerVisibility();
  const resolvedTheme: 'light' | 'dark' =
    theme === 'system'
      ? window.matchMedia('(prefers-color-scheme: dark)').matches
        ? 'dark'
        : 'light'
      : theme;
  const center = centerOf(candidates);
  const zoom = candidates.length > 0 ? 6 : 4;

  return (
    <div className="relative h-full w-full">
      <MapContainer
        key={resolvedTheme}
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
        {/* 候选航路层（决策 42：全部同色细线 + 选中高亮） */}
        {visible.has('routes') &&
          candidates.map((c) => {
            const points = c.route.points.map(
              (p) => [p.lat, p.lon] as [number, number],
            );
            const isSelected = selected?.index === c.index;
            return (
              <Polyline
                key={`${c.index}-${isSelected}`}
                positions={points}
                pathOptions={{
                  color: isSelected ? '#2563EB' : '#64748B',
                  weight: isSelected ? 4 : 1.5,
                  opacity: isSelected ? 1 : 0.5,
                }}
              />
            );
          })}
        {/* 航路点层（决策 41：Phase 9 全部同色；点击信息卡） */}
        {visible.has('waypoints') &&
          candidates.map((c) =>
            c.route.points.map((p) => (
              <CircleMarker
                key={`${c.index}-${p.index}`}
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
                    {p.from && <div>via {p.from}</div>}
                    <div>
                      {p.lat.toFixed(4)}, {p.lon.toFixed(4)}
                    </div>
                    <div className="text-muted-foreground">--</div>
                  </div>
                </Popup>
              </CircleMarker>
            )),
          )}
        {/* 备降机场层 */}
        {visible.has('alternates') &&
          alternates.map((a) => {
            const coord = alternateCoord(a);
            if (!coord) return null;
            return (
              <CircleMarker
                key={a.icao}
                center={coord}
                radius={5}
                pathOptions={{
                  color: '#D97706',
                  fillColor: '#D97706',
                  fillOpacity: 0.6,
                }}
              >
                <Tooltip>{`${a.icao} · ${Math.round(a.distance_nm)}NM`}</Tooltip>
              </CircleMarker>
            );
          })}
      </MapContainer>
      <LayerPanel visible={visible} onToggle={toggle} />
    </div>
  );
}
