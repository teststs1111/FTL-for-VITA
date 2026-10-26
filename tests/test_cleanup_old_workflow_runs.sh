#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
script="$root/scripts/cleanup-old-workflow-runs.sh"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/gh" <<'MOCK_GH'
#!/usr/bin/env bash
set -euo pipefail

state="${MOCK_STATE_DIR:?}"
if [[ "$*" == *"--method DELETE"* ]]; then
  count="$(cat "$state/delete-count" 2>/dev/null || echo 0)"
  echo "$((count + 1))" > "$state/delete-count"
  if (( count == 0 )); then
    echo "unexpected end of JSON input" >&2
    exit 1
  fi
  echo "gh: Not Found (HTTP 404)" >&2
  exit 1
fi

count="$(cat "$state/get-count" 2>/dev/null || echo 0)"
echo "$((count + 1))" > "$state/get-count"
if (( count == 0 )); then
  printf '{'
elif (( count == 1 )); then
  printf '{"workflow_runs":[{"id":123,"run_number":1}]}'
else
  printf '{"workflow_runs":[]}'
fi
MOCK_GH

cat > "$tmp/sleep" <<'MOCK_SLEEP'
#!/usr/bin/env bash
exit 0
MOCK_SLEEP
chmod +x "$tmp/gh" "$tmp/sleep"

output="$(PATH="$tmp:$PATH" MOCK_STATE_DIR="$tmp" REPO="test/repo" KEEP_LATEST=0 TARGET_WORKFLOW="host-build.yml" bash "$script" 2>&1)"
printf '%s\n' "$output"

grep -Fq "GitHub API returned invalid JSON" <<< "$output"
grep -Fq "Delete request for run #1 failed" <<< "$output"
grep -Fq "Run #1 (id 123) is already absent" <<< "$output"
grep -Fq "Workflow history cleanup completed." <<< "$output"
[[ "$(cat "$tmp/get-count")" == "3" ]]
[[ "$(cat "$tmp/delete-count")" == "2" ]]

echo "Workflow cleanup retry regression test passed."
