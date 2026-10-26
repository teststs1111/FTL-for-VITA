#!/usr/bin/env bash
set -euo pipefail

keep="${KEEP_LATEST:-30}"
target_workflow="${TARGET_WORKFLOW:-}"
repo="${REPO:?REPO is required}"

if ! [[ "$keep" =~ ^[0-9]+$ ]]; then
  echo "keep_latest must be a non-negative integer"
  exit 1
fi

# gh api can occasionally receive an incomplete response during a large
# cleanup. Retry transient failures and validate JSON before passing it to jq.
api_get_json() {
  local endpoint="$1"
  local attempt=1
  local delay=2
  local response

  while (( attempt <= 5 )); do
    if response="$(gh api -H "Accept: application/vnd.github+json" "$endpoint" 2>&1)"; then
      if jq -e 'type == "object"' >/dev/null 2>&1 <<< "$response"; then
        printf '%s\n' "$response"
        return 0
      fi
      echo "GitHub API returned invalid JSON for $endpoint (attempt $attempt/5)." >&2
    else
      echo "GitHub API request failed for $endpoint (attempt $attempt/5): $response" >&2
    fi

    if (( attempt < 5 )); then
      sleep "$delay"
      delay=$((delay * 2))
    fi
    ((attempt += 1))
  done

  echo "Could not fetch a valid JSON response for $endpoint after 5 attempts." >&2
  return 1
}

delete_run() {
  local run_id="$1"
  local run_number="$2"
  local endpoint="/repos/${repo}/actions/runs/${run_id}"
  local attempt=1
  local delay=2
  local response

  while (( attempt <= 5 )); do
    if response="$(gh api --method DELETE -H "Accept: application/vnd.github+json" "$endpoint" 2>&1)"; then
      echo "Deleted run #${run_number} (id ${run_id})"
      return 0
    fi

    # A lost/truncated success response can make a completed DELETE look like
    # a failure. A 404 on retry means the run is already gone.
    if grep -Eqi 'HTTP 404|Not Found' <<< "$response"; then
      echo "Run #${run_number} (id ${run_id}) is already absent; continuing."
      return 0
    fi

    echo "Delete request for run #${run_number} failed (attempt $attempt/5): $response" >&2
    if (( attempt < 5 )); then
      sleep "$delay"
      delay=$((delay * 2))
    fi
    ((attempt += 1))
  done

  echo "Failed to delete run #${run_number} (id ${run_id}) after 5 attempts." >&2
  return 1
}

delete_runs() {
  local workflow="$1"
  local workflow_name
  local runs_json
  local -a runs=()
  workflow_name="${workflow##*/}"
  echo "Cleaning workflow: $workflow_name"

  while true; do
    # Re-read page 1 after deletions so pagination cannot skip older runs.
    runs_json="$(api_get_json "/repos/${repo}/actions/workflows/${workflow_name}/runs?per_page=100&status=completed")"
    if ! jq -e '.workflow_runs | type == "array"' >/dev/null 2>&1 <<< "$runs_json"; then
      echo "GitHub API response for ${workflow_name} did not contain workflow_runs[]." >&2
      return 1
    fi
    mapfile -t runs < <(jq -r '.workflow_runs[] | [.id,.run_number] | @tsv' <<< "$runs_json")

    local count=${#runs[@]}
    if (( count <= keep )); then
      echo "Nothing more to delete ($count completed runs remain in the first page)."
      break
    fi

    local delete_count=0
    for ((i=keep; i<count; i++)); do
      local run_id run_number
      run_id="$(printf '%s' "${runs[i]}" | cut -f1)"
      run_number="$(printf '%s' "${runs[i]}" | cut -f2)"
      delete_run "$run_id" "$run_number"
      ((delete_count+=1))
      # Keep below GitHub's secondary content-generation rate limits.
      sleep 1
    done

    echo "Deleted $delete_count run(s); re-reading page 1."
  done
}

if [[ -n "$target_workflow" ]]; then
  delete_runs "$target_workflow"
else
  workflows_json="$(api_get_json "/repos/${repo}/actions/workflows?per_page=100")"
  if ! jq -e '.workflows | type == "array"' >/dev/null 2>&1 <<< "$workflows_json"; then
    echo "GitHub API response did not contain workflows[]." >&2
    exit 1
  fi
  mapfile -t workflows < <(jq -r '.workflows[] | select(.state == "active") | .path' <<< "$workflows_json")

  for workflow in "${workflows[@]}"; do
    delete_runs "$workflow"
  done
fi

echo "Workflow history cleanup completed."
