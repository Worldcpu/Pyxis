// JSON-RPC 客户端测试（决策 43：全 200 + error body；传输错误 400/404）。
// fetch 用 vi.stubGlobal mock——不触真实网络。

import { afterEach, describe, expect, it, vi } from 'vitest';

import { callRpc, RpcError } from './rpc';

afterEach(() => {
  vi.unstubAllGlobals();
});

function mockFetch(handler: (url: string, init: RequestInit) => unknown) {
  vi.stubGlobal(
    'fetch',
    vi.fn(async (url: string, init: RequestInit) => {
      const body = await handler(url, init);
      return new Response(JSON.stringify(body), {
        status: 200,
        headers: { 'Content-Type': 'application/json' },
      });
    }),
  );
}

describe('callRpc: 成功路径', () => {
  it('返回 result 且请求信封正确（jsonrpc/method/params/id）', async () => {
    let seen: unknown;
    mockFetch((url, init) => {
      seen = { url, body: JSON.parse(String(init.body)) };
      return { jsonrpc: '2.0', result: { ok: true }, id: 1 };
    });
    const result = await callRpc<{ ok: boolean }>('plan.routes', {
      departure: 'ZUCK',
    });
    expect(result).toEqual({ ok: true });
    const body = (seen as { body: { jsonrpc: string; method: string; id: number } })
      .body;
    expect(body.jsonrpc).toBe('2.0');
    expect(body.method).toBe('plan.routes');
    expect(typeof body.id).toBe('number');
    expect((seen as { url: string }).url).toBe('http://127.0.0.1:19100/rpc');
  });

  it('id 逐次递增（多请求可关联）', async () => {
    const ids: number[] = [];
    mockFetch((_url, init) => {
      ids.push(JSON.parse(String(init.body)).id);
      return { jsonrpc: '2.0', result: null, id: ids[ids.length - 1] };
    });
    await callRpc('a', {});
    await callRpc('b', {});
    expect(ids[1]).toBe(ids[0] + 1);
  });
});

describe('callRpc: 错误路径', () => {
  it('业务错误（全 200 + error body）→ RpcError 带 code/message', async () => {
    mockFetch(() => ({
      jsonrpc: '2.0',
      error: { code: -32000, message: 'navdata 缺失' },
      id: 1,
    }));
    await expect(callRpc('plan.routes', {})).rejects.toMatchObject({
      code: -32000,
      message: 'navdata 缺失',
    });
  });

  it('传输错误（HTTP 404）→ RpcError code=404', async () => {
    vi.stubGlobal(
      'fetch',
      vi.fn(async () => new Response('not found', { status: 404 })),
    );
    const err = await callRpc('plan.routes', {}).catch((e: unknown) => e);
    expect(err).toBeInstanceOf(RpcError);
    expect((err as RpcError).code).toBe(404);
  });

  it('网络失败（fetch reject）→ 原样透传', async () => {
    vi.stubGlobal(
      'fetch',
      vi.fn(async () => {
        throw new TypeError('Failed to fetch');
      }),
    );
    await expect(callRpc('plan.routes', {})).rejects.toThrow('Failed to fetch');
  });

  it('响应非 JSON → 抛出而非静默', async () => {
    vi.stubGlobal(
      'fetch',
      vi.fn(async () => new Response('oops', { status: 200 })),
    );
    await expect(callRpc('plan.routes', {})).rejects.toThrow();
  });

  it('支持 AbortSignal（超时/取消）', async () => {
    const controller = new AbortController();
    mockFetch((_url, init) => {
      expect((init.signal as AbortSignal | undefined)?.aborted).toBe(false);
      return { jsonrpc: '2.0', result: null, id: 1 };
    });
    await callRpc('plan.routes', {}, { signal: controller.signal });
  });
});
