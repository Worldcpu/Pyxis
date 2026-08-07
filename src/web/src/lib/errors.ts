// RPC 错误码 → i18n key 映射（ui-spec §6：错误条按码映射中文消息）。

/** 业务/协议错误码 → 语言包 key（未知码兜底 errors.unknown）。 */
export function rpcErrorKey(code: number): string {
  switch (code) {
    case 400:
      return 'errors.badRequest';
    case 404:
      return 'errors.notFound';
    case 422:
      return 'errors.noSolution';
    case -32000:
      return 'errors.internal';
    case -32601:
      return 'errors.methodNotFound';
    case -32600:
      return 'errors.invalidRequest';
    case -32700:
      return 'errors.parseError';
    default:
      return 'errors.unknown';
  }
}
