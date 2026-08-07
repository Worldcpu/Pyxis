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

/** 超时/中止错误码（非 JSON-RPC 标准——前端专用区分）。 */
export const RPC_TIMEOUT_CODE = -32001;

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
  let res: Response;
  try {
    res = await fetch(opts?.url ?? RPC_URL, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ jsonrpc: '2.0', method, params, id: nextId++ }),
      signal,
    });
  } catch (e) {
    // 审查修复：超时/中止（DOMException TimeoutError/AbortError）此前
    // 被误报为 offline——转 RpcError 供调用方区分。
    if (e instanceof DOMException && e.name === 'TimeoutError') {
      throw new RpcError(RPC_TIMEOUT_CODE, '请求超时');
    }
    if (e instanceof DOMException && e.name === 'AbortError') {
      throw new RpcError(RPC_TIMEOUT_CODE, '请求已取消');
    }
    throw e;
  }

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
