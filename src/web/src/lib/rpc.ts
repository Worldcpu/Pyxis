// JSON-RPC 2.0 客户端（决策 43：全 200 + error body；传输错误 400/404）。
// 单源 URL 常量；id 自增保证请求/响应可关联。

/** px_server 默认端口（决策 G4）。 */
export const RPC_URL = 'http://127.0.0.1:19100/rpc';

/** RPC 业务/传输错误（code = JSON-RPC 错误码或 HTTP 状态码）。 */
export class RpcError extends Error {
  readonly code: number;
  constructor(code: number, message: string) {
    super(message);
    this.code = code;
  }
}

let nextId = 1;

/** 默认超时（挂起后端防无限 loading——审查修复）。 */
const DEFAULT_TIMEOUT_MS = 15_000;

/**
 * 调用 JSON-RPC 方法。
 * @throws {RpcError} 业务错误（error body）或传输错误（HTTP 非 200）
 */
export async function callRpc<T>(
  method: string,
  params: object,
  opts?: { signal?: AbortSignal; url?: string },
): Promise<T> {
  const signal =
    opts?.signal ?? AbortSignal.timeout(DEFAULT_TIMEOUT_MS);
  const res = await fetch(opts?.url ?? RPC_URL, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ jsonrpc: '2.0', method, params, id: nextId++ }),
    signal,
  });

  if (!res.ok) {
    const text = await res.text().catch(() => '');
    throw new RpcError(res.status, text.slice(0, 200) || `HTTP ${res.status}`);
  }

  let body: { result?: unknown; error?: { code: number; message: string } };
  try {
    body = await res.json();
  } catch {
    throw new RpcError(-32700, '响应不是合法 JSON');
  }

  if (body.error !== undefined) {
    throw new RpcError(body.error.code, body.error.message);
  }
  return body.result as T;
}
