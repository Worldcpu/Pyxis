// 页面级冒烟测试（S9.1）：两阶段流程（表单 → Task）+ 工具栏列 +
// RPC 集成。react-leaflet mock（jsdom 无 DOM 测量）；RPC 用 vi.stubGlobal mock。

import { act, render, screen, waitFor } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import i18n from 'i18next';
import { MemoryRouter } from 'react-router-dom';
import { afterEach, beforeAll, describe, expect, it, vi } from 'vitest';

import { FlightPlanPage } from './flightplan-page';
import { ThemeProvider } from '../components/theme-provider';
import { TooltipProvider } from '../components/ui/tooltip';
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
  useMap: () => ({
    fitBounds: vi.fn(),
    getZoom: () => 6,
    on: vi.fn(),
    off: vi.fn(),
  }),
}));

const ROUTE_POINTS = [
  { ident: 'ZUCK', lat: 29.7192, lon: 106.6417, segment_index: -1 },
  { ident: 'TONIN', lat: 28.5, lon: 104.2, via: 'W80', segment_index: 0 },
  { ident: 'ZBAA', lat: 40.0801, lon: 116.5846, segment_index: -1 },
];

const VALID_FLIGHTPLAN = {
  route: {
    points: ROUTE_POINTS,
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

const ANALYZE_OK = {
  valid: true,
  cycle: 2601,
  distance_nm: 580.5,
  points: ROUTE_POINTS,
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
        case 'profile.list':
          return jsonRpc([], body.id);
        case 'plan.alternates':
          return jsonRpc(
            [{ icao: 'ZLXY', distance_nm: 180, lat: 34.4471, lon: 108.7516 }],
            body.id,
          );
        case 'plan.analyze':
          return jsonRpc(ANALYZE_OK, body.id);
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
    <TooltipProvider>
      <ThemeProvider defaultTheme="light">
        <MemoryRouter>
          <FlightPlanPage />
        </MemoryRouter>
      </ThemeProvider>
    </TooltipProvider>,
  );
}

describe('FlightPlanPage: 两阶段流程（S9.1 D56）', () => {
  it('① 表单：核心分区标题渲染', async () => {
    setupFetch();
    renderPage();
    await waitFor(() => {
      expect(screen.getByText('Flight Info')).toBeInTheDocument();
    });
    expect(screen.getByText('Aircraft')).toBeInTheDocument();
    expect(screen.getByText('Selections')).toBeInTheDocument();
    expect(screen.getByText('Route')).toBeInTheDocument();
  });

  it('工具栏列：Layers 下拉（三数据图层开关）+ 主题切换（T3）', async () => {
    setupFetch();
    renderPage();
    const user = userEvent.setup();
    // Layers 按钮展开 → 三个数据图层开关（D51：无底图开关）。
    await user.click(screen.getByRole('button', { name: '图层' }));
    expect(
      screen.getByRole('switch', { name: '候选航路' }),
    ).toBeInTheDocument();
    expect(screen.getByRole('switch', { name: '航路点' })).toBeInTheDocument();
    expect(
      screen.getByRole('switch', { name: '备降机场' }),
    ).toBeInTheDocument();
    // 同列主题切换（D58）。
    expect(
      screen.getByRole('button', { name: /点击切换至/ }),
    ).toBeInTheDocument();
  });

  it('未分析时生成按钮禁用（D54：enabled iff 规划航路）', async () => {
    setupFetch();
    renderPage();
    await waitFor(() => {
      expect(
        screen.getByRole('button', { name: '生成计划' }),
      ).toBeInTheDocument();
    });
    expect(
      screen.getByRole('button', { name: '生成计划' }),
    ).toBeDisabled();
  });

  it('分析成功 → 信息框（AIRAC + 距离）→ 生成 → Flight Task（D54/D56）', async () => {
    setupFetch();
    renderPage();
    const user = userEvent.setup();
    // 表单填写（机型两级选择 + 航路串）。
    await user.type(screen.getAllByPlaceholderText('4 字 ICAO')[0], 'ZUCK');
    await user.type(screen.getAllByPlaceholderText('4 字 ICAO')[1], 'ZBAA');
    await user.click(screen.getAllByRole('combobox')[0]);
    await user.click(await screen.findByText('A320'));
    await user.click(screen.getAllByRole('combobox')[1]);
    await user.click(await screen.findByText('Fenix A320 CFM'));
    await user.type(
      screen.getByRole('textbox', { name: '航路串' }),
      'ZUCK TONIN W80 MAKET ZBAA',
    );
    // 分析 → 有效性信息框（D54）。
    await user.click(screen.getByRole('button', { name: '分析航路' }));
    await waitFor(() => {
      expect(
        screen.getByText('已在 AIRAC 2601 下验证通过，航路距离 581 nm'),
      ).toBeInTheDocument();
    });
    // 生成 → Task（D56 两阶段）。
    await act(async () => {
      await user.click(screen.getByRole('button', { name: '生成计划' }));
    });
    await waitFor(() => {
      expect(screen.getByText('您的航班任务')).toBeInTheDocument();
    });
    expect(screen.getByText('2601')).toBeInTheDocument();
    expect(screen.getByText('FL350 / 10668 m')).toBeInTheDocument();
  });

  it('SimBrief 导入成功 → 直达 Flight Task（D52）', async () => {
    setupFetch();
    renderPage();
    const user = userEvent.setup();
    // 切到 SimBrief 标签 → 输入 pilotid → 导入。
    await user.click(screen.getByRole('tab', { name: 'SimBrief 导入' }));
    // mock fetch：非 RPC URL → 返回合法 OFP XML。
    const rpcFetch = vi.mocked(fetch);
    rpcFetch.mockImplementation(async (url: string | URL | Request, init?: RequestInit) => {
      const u = String(url);
      if (u.includes('simbrief.com')) {
        return new Response(
          `<?xml version="1.0"?><OFP><origin><icao_code>ZUCK</icao_code></origin>
           <destination><icao_code>ZBAA</icao_code></destination>
           <general><route>ZUCK TONIN W80 MAKET ZBAA</route></general>
           <aircraft><icao_code>A320</icao_code></aircraft></OFP>`,
          { status: 200 },
        );
      }
      const body = JSON.parse(String(init?.body)) as { method: string; id: number };
      switch (body.method) {
        case 'airframe.list':
          return jsonRpc([AIRFRAME], body.id);
        case 'list_cycles':
          return jsonRpc({ cycles: [2601] }, body.id);
        case 'profile.list':
          return jsonRpc([], body.id);
        case 'plan.analyze':
          return jsonRpc(ANALYZE_OK, body.id);
        case 'plan.routes':
          return jsonRpc(
            [
              {
                index: 0,
                route_string: 'ZUCK TONIN W80 MAKET ZBAA',
                total_distance_nm: 580,
                points: ROUTE_POINTS,
              },
            ],
            body.id,
          );
        case 'plan.generate':
          return jsonRpc(VALID_FLIGHTPLAN, body.id);
        default:
          return jsonRpc(null, body.id);
      }
    });
    await user.type(
      screen.getByLabelText('Pilot ID'),
      '123456',
    );
    await user.click(screen.getByRole('button', { name: '导入' }));
    await waitFor(() => {
      expect(screen.getByText('您的航班任务')).toBeInTheDocument();
    });
  });

  it('SimBrief 导入失败 → 分类 Dialog（parse）', async () => {
    vi.stubGlobal(
      'fetch',
      vi.fn(async (url: string | URL | Request, init?: RequestInit) => {
        const u = String(url);
        if (u.includes('simbrief.com')) {
          return new Response('<html><body>Not an OFP</body></html>', {
            status: 200,
          });
        }
        const body = JSON.parse(String(init?.body)) as {
          method: string;
          id: number;
        };
        switch (body.method) {
          case 'airframe.list':
            return jsonRpc([AIRFRAME], body.id);
          case 'list_cycles':
            return jsonRpc({ cycles: [2601] }, body.id);
          case 'profile.list':
            return jsonRpc([], body.id);
          default:
            return jsonRpc(null, body.id);
        }
      }),
    );
    renderPage();
    const user = userEvent.setup();
    await user.click(screen.getByRole('tab', { name: 'SimBrief 导入' }));
    await user.type(screen.getByLabelText('Pilot ID'), '123456');
    await user.click(screen.getByRole('button', { name: '导入' }));
    await waitFor(() => {
      expect(screen.getByText('SimBrief 导入失败')).toBeInTheDocument();
    });
    expect(
      screen.getByText('OFP 解析失败，请确认复制的是完整页面内容。'),
    ).toBeInTheDocument();
  });

  it('Suggest Route：生成候选卡 + 蓝色徽章 + 替换确认（D55 rev.）', async () => {
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
          case 'profile.list':
            return jsonRpc([{ name: 'VFR-low', k: 5 }], body.id);
          case 'plan.alternates':
            return jsonRpc([], body.id);
          case 'plan.analyze':
            return jsonRpc(ANALYZE_OK, body.id);
          case 'plan.routes':
            return jsonRpc(
              [
                {
                  index: 0,
                  route_string: 'ZUCK TONIN W80 MAKET ZBAA',
                  total_distance_nm: 580,
                  points: ROUTE_POINTS,
                },
                {
                  index: 1,
                  route_string: 'ZUCK KAKAK W90 BIDIB ZBAA',
                  total_distance_nm: 610,
                  points: ROUTE_POINTS,
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
    renderPage();
    const user = userEvent.setup();
    // 表单：起降场 + 航路串 + 分析（建立规划航路）。
    await user.type(screen.getAllByPlaceholderText('4 字 ICAO')[0], 'ZUCK');
    await user.type(screen.getAllByPlaceholderText('4 字 ICAO')[1], 'ZBAA');
    await user.type(
      screen.getByRole('textbox', { name: '航路串' }),
      'ZUCK TONIN W80 MAKET ZBAA',
    );
    await user.click(screen.getByRole('button', { name: '分析航路' }));
    await waitFor(() => {
      expect(screen.getByText(/已在 AIRAC/)).toBeInTheDocument();
    });
    // 生成候选 → 两张卡片；匹配卡带蓝色徽章。
    await user.click(screen.getByRole('button', { name: '生成候选' }));
    await waitFor(() => {
      expect(
        screen.getByText('ZUCK KAKAK W90 BIDIB ZBAA'),
      ).toBeInTheDocument();
    });
    expect(screen.getByText('Route input route')).toBeInTheDocument();
    // 悬停非匹配卡（不崩溃）；点击 → 替换确认 Dialog。
    await user.hover(screen.getByText('ZUCK KAKAK W90 BIDIB ZBAA'));
    await user.click(screen.getByText('ZUCK KAKAK W90 BIDIB ZBAA'));
    await waitFor(() => {
      expect(screen.getByText('替换当前航路？')).toBeInTheDocument();
    });
    // 确认替换 → Route 输入回填候选航路；徽章跟随新匹配（候选 2 = 输入）。
    await user.click(screen.getByRole('button', { name: '替换' }));
    await waitFor(() => {
      expect(
        screen.getByRole('textbox', { name: '航路串' }),
      ).toHaveValue('ZUCK KAKAK W90 BIDIB ZBAA');
    });
    expect(screen.getByText('Route input route')).toBeInTheDocument();
    // 生成按钮保持可用（候选即规划航路，免重分析）。
    expect(
      screen.getByRole('button', { name: '生成计划' }),
    ).toBeEnabled();
  });

  it('编辑 Route 输入后旧分析失效（评审修复：生成门禁跟随输入）', async () => {
    setupFetch();
    renderPage();
    const user = userEvent.setup();
    await user.type(
      screen.getByRole('textbox', { name: '航路串' }),
      'ZUCK TONIN W80 MAKET ZBAA',
    );
    await user.click(screen.getByRole('button', { name: '分析航路' }));
    await waitFor(() => {
      expect(screen.getByText(/已在 AIRAC/)).toBeInTheDocument();
    });
    expect(
      screen.getByRole('button', { name: '生成计划' }),
    ).toBeEnabled();
    // 编辑航路串 → 旧分析失效：信息框消失 + 生成禁用。
    await user.type(
      screen.getByRole('textbox', { name: '航路串' }),
      ' EXTRA',
    );
    await waitFor(() => {
      expect(
        screen.getByRole('button', { name: '生成计划' }),
      ).toBeDisabled();
    });
    expect(screen.queryByText(/已在 AIRAC/)).not.toBeInTheDocument();
  });

  it('分析失败 → 逐条错误列表（role=alert）', async () => {
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
          case 'profile.list':
            return jsonRpc([], body.id);
          case 'plan.analyze':
            return jsonRpc(
              {
                valid: false,
                cycle: 2601,
                errors: [{ message: "waypoint 'XXXX' not found" }],
              },
              body.id,
            );
          default:
            return jsonRpc(null, body.id);
        }
      }),
    );
    renderPage();
    const user = userEvent.setup();
    await user.type(
      screen.getByRole('textbox', { name: '航路串' }),
      'ZUCK XXXX ZBAA',
    );
    await user.click(screen.getByRole('button', { name: '分析航路' }));
    await waitFor(() => {
      expect(
        screen.getByRole('alert'),
      ).toBeInTheDocument();
    });
    expect(screen.getByText("waypoint 'XXXX' not found")).toBeInTheDocument();
  });
});
