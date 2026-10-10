#!/usr/bin/env bash
# Install the commit gate (tools/gate.py) as this repository's pre-commit hook. The hooks folder is
# shared by every worktree; the hook runs the gate of the checkout being committed, and does nothing
# in a checkout that has no tools/gate.py. Skip it once with `git commit --no-verify`.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HOOKS="$(git -C "$ROOT" rev-parse --path-format=absolute --git-common-dir)/hooks"
HOOK="$HOOKS/pre-commit"
MARK="installed by tools/install_hooks.sh"
if [ -e "$HOOK" ] && ! grep -q "$MARK" "$HOOK"; then
  echo "$HOOK exists and is not ours; move it away first" >&2
  exit 1
fi
mkdir -p "$HOOKS"
cat > "$HOOK" <<HOOK
#!/bin/sh
# The commit gate, $MARK: fast checks of what is being committed (tools/gate.py).
top=\$(git rev-parse --show-toplevel) || exit 0
[ -f "\$top/tools/gate.py" ] || exit 0
exec python3 "\$top/tools/gate.py"
HOOK
chmod +x "$HOOK"
echo "pre-commit hook installed: $HOOK"
