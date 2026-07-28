#!/usr/bin/env bash
# 同步 lib/bravofinder 子模块到 Worldcpu fork 最新 v3。
# 用法: tools/sync-bf-engine.sh [--commit]

set -euo pipefail
cd "$(dirname "$0")/.."

SUBMODULE=lib/bravofinder

echo "=== 拉取 Worldcpu/BravoFinder v3 ==="
git -C "$SUBMODULE" fetch origin v3

OLD=$(git -C "$SUBMODULE" rev-parse HEAD)
NEW=$(git -C "$SUBMODULE" rev-parse origin/v3)

if [ "$OLD" = "$NEW" ]; then
  echo "已是最新 ($(git -C "$SUBMODULE" rev-parse --short HEAD))"
  exit 0
fi

echo "更新: $(git -C "$SUBMODULE" rev-parse --short $OLD) → $(git -C "$SUBMODULE" rev-parse --short $NEW)"
echo ""
git -C "$SUBMODULE" log --oneline "${OLD}..${NEW}" | head -20

git -C "$SUBMODULE" merge --ff-only origin/v3

echo ""
echo "=== 更新子模块指针 ==="
git add "$SUBMODULE"

if [ "${1:-}" = "--commit" ]; then
  git commit -m "chore(submodule): update lib/bravofinder to $(git -C "$SUBMODULE" rev-parse --short HEAD)

$(git -C "$SUBMODULE" log --oneline "${OLD}..${NEW}" | sed 's/^/- /')"
  echo "已提交。"
else
  echo "子模块已更新（未提交）。使用 --commit 自动提交。"
fi
