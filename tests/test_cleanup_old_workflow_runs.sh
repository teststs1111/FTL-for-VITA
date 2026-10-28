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
grep -Fq "Workflow cleanup completed." <<< "$output"
[[ "$(cat "$tmp/get-count")" == "3" ]]
[[ "$(cat "$tmp/delete-count")" == "2" ]]

echo "Workflow cleanup retry regression test passed."

# Exactly 30 completed runs must be retained without any DELETE request.
mkdir -p "$tmp/keep30"
cat > "$tmp/keep30/gh" <<'MOCK_GH_KEEP30'
#!/usr/bin/env bash
set -euo pipefail
if [[ "$*" == *"--method DELETE"* ]]; then
  echo "unexpected DELETE while retaining 30 runs" >&2
  exit 99
fi
printf '{"workflow_runs":['
for ((i=1; i<=30; i++)); do
  if (( i > 1 )); then printf ','; fi
  printf '{"id":%d,"run_number":%d}' "$i" "$i"
done
printf ']}\n'
MOCK_GH_KEEP30
chmod +x "$tmp/keep30/gh"
keep_output="$(PATH="$tmp/keep30:$PATH" REPO="test/repo" KEEP_LATEST=30 TARGET_WORKFLOW="host-build.yml" bash "$script" 2>&1)"
grep -Fq "Nothing more to delete (30 completed runs remain" <<< "$keep_output"
echo "Workflow cleanup keep-30 regression test passed."

# A malformed workflow_runs field must fail closed before deleting anything.
mkdir -p "$tmp/malformed"
cat > "$tmp/malformed/gh" <<'MOCK_GH_MALFORMED'
#!/usr/bin/env bash
set -euo pipefail
if [[ "$*" == *"--method DELETE"* ]]; then
  echo "unexpected DELETE for malformed response" >&2
  exit 99
fi
printf '{"workflow_runs":{}}\n'
MOCK_GH_MALFORMED
chmod +x "$tmp/malformed/gh"
if PATH="$tmp/malformed:$PATH" REPO="test/repo" KEEP_LATEST=0 TARGET_WORKFLOW="host-build.yml" bash "$script" >"$tmp/malformed/out" 2>&1; then
  echo "Malformed workflow_runs response unexpectedly succeeded" >&2
  exit 1
fi
grep -Fq "did not contain workflow_runs[]" "$tmp/malformed/out"
echo "Workflow cleanup malformed-response regression test passed."

# A permission failure (403) is not equivalent to an already-deleted run.
mkdir -p "$tmp/forbidden"
cat > "$tmp/forbidden/gh" <<'MOCK_GH_FORBIDDEN'
#!/usr/bin/env bash
set -euo pipefail
state="${MOCK_STATE_DIR:?}"
if [[ "$*" == *"--method DELETE"* ]]; then
  count="$(cat "$state/delete-count" 2>/dev/null || echo 0)"
  echo "$((count + 1))" > "$state/delete-count"
  echo "gh: HTTP 403: Resource not accessible by integration" >&2
  exit 1
fi
printf '{"workflow_runs":[{"id":456,"run_number":2}]}\n'
MOCK_GH_FORBIDDEN
chmod +x "$tmp/forbidden/gh"
if PATH="$tmp/forbidden:$tmp:$PATH" MOCK_STATE_DIR="$tmp/forbidden" REPO="test/repo" KEEP_LATEST=0 TARGET_WORKFLOW="host-build.yml" bash "$script" >"$tmp/forbidden/out" 2>&1; then
  echo "HTTP 403 unexpectedly treated as a successful delete" >&2
  exit 1
fi
[[ "$(cat "$tmp/forbidden/delete-count")" == "5" ]]
grep -Fq "Failed to delete run #2" "$tmp/forbidden/out"
echo "Workflow cleanup permission-failure regression test passed."
