// RPC 错误码 → i18n key 映射测试（ui-spec §6：400/404/422/-32000/-32601 等）。

import { describe, expect, it } from 'vitest';

import { rpcErrorKey } from './errors';

describe('rpcErrorKey: 已知码映射', () => {
  it('400 → errors.badRequest（参数错）', () => {
    expect(rpcErrorKey(400)).toBe('errors.badRequest');
  });
  it('404 → errors.notFound（查无）', () => {
    expect(rpcErrorKey(404)).toBe('errors.notFound');
  });
  it('422 → errors.noSolution（无解）', () => {
    expect(rpcErrorKey(422)).toBe('errors.noSolution');
  });
  it('-32000 → errors.internal（内部）', () => {
    expect(rpcErrorKey(-32000)).toBe('errors.internal');
  });
  it('-32601 → errors.methodNotFound', () => {
    expect(rpcErrorKey(-32601)).toBe('errors.methodNotFound');
  });
  it('-32600/-32700 协议码', () => {
    expect(rpcErrorKey(-32600)).toBe('errors.invalidRequest');
    expect(rpcErrorKey(-32700)).toBe('errors.parseError');
  });
});

describe('rpcErrorKey: 未知码兜底', () => {
  it('其他 → errors.unknown', () => {
    expect(rpcErrorKey(500)).toBe('errors.unknown');
    expect(rpcErrorKey(-1)).toBe('errors.unknown');
  });
});
