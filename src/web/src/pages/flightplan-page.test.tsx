// 页面级冒烟测试：三子阶段切换 + RPC 集成（决策 34/42）。
// react-leaflet mock（jsdom 无 DOM 测量）；RPC 用 vi.stubGlobal mock。

import { act, render, screen, waitFor } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import i18n from 'i18next';
import { MemoryRouter } from 'react-router-dom';
import { afterEach, beforeAll, describe, expect, it, vi } from 'vitest';

import { FlightPlanPage } from './flightplan-page';
import { ThemeProvider } from '../components/theme-provider';
// 加载项目 i18n 配置（zh 资源注册到同一 i18next 单例）。
import '../i18n';

// Leaflet 在 jsdom 无法初始化（需要真实 DOM 测量）——地图层 mock 掉。
vi.mock('react-leaflet', () => ({
  MapContainer: ({ children }: { children: React.ReactNode }) => (
    <div data-testid="map">{children}</div>
  ),
  TileLayer: () => null,
  Polyline: () => null,
  CircleMarker: () => null,
  Tooltip: () => null,
  Popup: () => null,
}));

const VALID_FLIGHTPLAN = {
  route: {
    points: [
      { ident: 'ZUCK', lat: 29.7192, lon: 106.6417, index: 0 },
      { ident: 'TONIN', lat: 28.5, lon: 104.2, index: 1 },
      { ident: 'ZBAA', lat: 40.0801, lon: 116.5846, index: 2 },
    ],
  },
  altitude: { fl: 350, meters: 10668, manual: false, rationale: '' },
  weights: {
    dow_kg: 41000,
    zfw_kg: 50000,
    mzfw_kg: 61000,
    mtow_kg: 77000,
    mlw_kg: 66000,
  },
  checks: { status: 'ok', warnings: [] },
  mora_checked: true,
};

function jsonRpc(result: unknown, id: number) {
  return new Response(JSON.stringify({ jsonrpc: '2.0', result, id }), {
    status: 200,
    headers: { 'Content-Type': 'application/json' },
  });
}

const AIRFRAME = {
  type: 'A320',
  variant: 'Fenix A320 CFM',
  perf_source: 'fcom',
  dow_kg: 41000,
  mzfw_kg: 61000,
  mtow_kg: 77000,
  mlw_kg: 66000,
  service_ceiling_ft: 39000,
  unit_pax_kg: 75,
  unit_bag_kg: 15,
  cruise_speed_kt: 437,
};

beforeAll(async () => {
  await i18n.init();
});

afterEach(() => {
  vi.unstubAllGlobals();
});

function setupFetch() {
  vi.stubGlobal(
    'fetch',
    vi.fn(async (_url: string, init: RequestInit) => {
      const body = JSON.parse(String(init.body)) as {
        method: string;
        id: number;
      };
      switch (body.method) {
        case 'airframe.list':
          return jsonRpc([AIRFRAME], body.id);
        case 'list_cycles':
          return jsonRpc({ cycles: [2601] }, body.id);
        case 'plan.alternates':
          return jsonRpc(
            [
              {
                icao: 'ZLXY',
                distance_nm: 180,
                route: { points: [{ lat: 34.4471, lon: 108.7516 }] },
              },
            ],
            body.id,
          );
        case 'plan.routes':
          return jsonRpc(
            [
              {
                index: 0,
                route_string: 'ZUCK TONIN W80 MAKET ZBAA',
                route: VALID_FLIGHTPLAN.route,
                altitude: VALID_FLIGHTPLAN.altitude,
                distance_nm: 580,
              },
            ],
            body.id,
          );
        case 'plan.generate':
          return jsonRpc(VALID_FLIGHTPLAN, body.id);
        default:
          return jsonRpc(null, body.id);
      }
    }),
  );
}

function renderPage() {
  return render(
    <ThemeProvider defaultTheme="light">
      <MemoryRouter>
        <FlightPlanPage />
      </MemoryRouter>
    </ThemeProvider>,
  );
}

describe('FlightPlanPage: 三子阶段流程', () => {
  it('① 表单：六分区标题渲染', async () => {
    setupFetch();
    renderPage();
    await waitFor(() => {
      expect(screen.getByText('Flight Info')).toBeInTheDocument();
    });
    expect(screen.getByText('Aircraft')).toBeInTheDocument();
    expect(screen.getByText('Selections')).toBeInTheDocument();
    expect(screen.getByText('Route')).toBeInTheDocument();
  });

  it('表单 → ② 候选列表（route_string 显示）', async () => {
    setupFetch();
    renderPage();
    const user = userEvent.setup();
    await user.type(screen.getAllByPlaceholderText('4 字 ICAO')[0], 'ZUCK');
    await user.type(screen.getAllByPlaceholderText('4 字 ICAO')[1], 'ZBAA');
    await user.click(screen.getByRole('button', { name: '生成候选' }));
    await waitFor(() => {
      expect(
        screen.getByText('ZUCK TONIN W80 MAKET ZBAA'),
      ).toBeInTheDocument();
    });
  });

  it('② 候选 → ③ Flight Task（航班任务 + AIRAC 周期）', async () => {
    setupFetch();
    renderPage();
    const user = userEvent.setup();
    // 表单填写（含机型选择）→ 生成候选 → 生成计划。
    await user.type(screen.getAllByPlaceholderText('4 字 ICAO')[0], 'ZUCK');
    await user.type(screen.getAllByPlaceholderText('4 字 ICAO')[1], 'ZBAA');
    // 机型两级选择（决策 8：type → variant）。
    await user.click(screen.getAllByRole('combobox')[0]);
    await user.click(await screen.findByText('A320'));
    await user.click(screen.getAllByRole('combobox')[1]);
    await user.click(await screen.findByText('Fenix A320 CFM'));
    await user.click(screen.getByRole('button', { name: '生成候选' }));
    await screen.findByText('ZUCK TONIN W80 MAKET ZBAA');
    await act(async () => {
      await user.click(screen.getByRole('button', { name: '生成计划' }));
    });
    await waitFor(() => {
      expect(screen.getByText('您的航班任务')).toBeInTheDocument();
    });
    // AIRAC 周期（决策 6：list_cycles 取）。
    expect(screen.getByText('2601')).toBeInTheDocument();
    // 巡航双单位（决策 24）。
    expect(screen.getByText('FL350 / 10668 m')).toBeInTheDocument();
  });
});
