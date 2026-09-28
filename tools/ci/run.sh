#!/usr/bin/env bash
# One definition of every CI job. GitHub Actions and local runs call this same
# script, so a job that passes here passes there.
#
#   tools/ci/run.sh gcc | clang | sanitizers | text-evaluation | web-browser
#   tools/ci/run.sh quick    gcc + text-evaluation            (before every push)
#   tools/ci/run.sh full     gcc, clang, sanitizers, text-evaluation, in order
#
# Jobs never overlap. On the 2-core VM the GCC and sanitizer suites must not run
# together: timing gates such as the 30k-number check are deliberate limits.
# Build trees go to build-ci-* (ignored), so a developer's build/ is untouched.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
cd "$ROOT"
JOBS=${CI_JOBS:-2}
OUT=${CI_BUILD_ROOT:-build-ci}
ARTIFACTS=${CI_ARTIFACTS:-artifacts/evaluation}

build_and_test() {  # build-dir compiler [cmake args...]
  local dir=$1 compiler=$2
  shift 2
  cmake -S . -B "$dir" -DCMAKE_C_COMPILER="$compiler" -DCMAKE_BUILD_TYPE=Debug "$@"
  cmake --build "$dir" --parallel "$JOBS"
  ctest --test-dir "$dir" --output-on-failure
}

release_build() {
  cmake -S . -B "$OUT-release" -DCMAKE_BUILD_TYPE=Release
  cmake --build "$OUT-release" --parallel "$JOBS"
}

job_gcc() { build_and_test "$OUT-gcc" gcc; }

job_clang() { build_and_test "$OUT-clang" clang; }

job_sanitizers() {
  ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    build_and_test "$OUT-sanitizers" clang -DTTF_ENABLE_ASAN=ON -DTTF_ENABLE_UBSAN=ON
}

job_text_evaluation() {
  local ttf="$OUT-release/ttf"
  release_build
  ctest --test-dir "$OUT-release" --output-on-failure \
    -R 'cli_stream_json|evaluation|lexicon|external_|korean_release_|context_score_cache|context_cache_compatibility'
  evaluate() {  # corpus output-name
    python3 tools/evaluation/evaluate.py --ttf "$ttf" --resources resources \
      --corpus "tests/evaluation/$1" --output-dir "$ARTIFACTS/$2"
  }
  evaluate cases.jsonl regression
  python3 tools/evaluation/audit_references.py --self-test --output "$ARTIFACTS/reference-audit.json"
  evaluate domain_cases_v8.jsonl domain
  # Advisory: the challenge corpus reports text mismatches (exit 1) without failing.
  evaluate challenge_cases.jsonl challenge || test "$?" -eq 1
  evaluate advanced_cases.jsonl advanced
  evaluate protected_cases.jsonl protected
  evaluate finance_ranges_v7.jsonl finance-v7
  evaluate content_v7.jsonl content-v7
  evaluate multiword_v8.jsonl multiword-v8
  python3 tools/lexicon/manage.py build --check
  python3 tools/lexicon/manage.py inventory --output "$ARTIFACTS/lexicon-inventory.json"
  evaluate lexicon_v10.jsonl lexicon-v10
  python3 tools/cjk/generate.py build --unihan data/cjk --out resources/languages/ko-KR/cjk --check
  evaluate cjk_reading_v22.jsonl cjk-v22
  evaluate scaled_currency_v12.jsonl scaled-currency-v12
  evaluate english_dates_v13.jsonl english-dates-v13
  evaluate english_forms_v14.jsonl english-forms-v14
  evaluate korean_counters_v15.jsonl korean-counters-v15
  evaluate phone_cases_v18.jsonl phone-v18
  evaluate user_patterns_v20.jsonl user-patterns-v20
}

job_web_browser() {
  local port=${CI_WEB_PORT:-8765} pid
  if [ -z "${TTF_PLAYWRIGHT_MODULE:-}" ] && [ ! -e tests/web/node_modules/playwright ]; then
    echo "web-browser: Playwright is not installed." >&2
    echo "  npm ci --prefix tests/web && tests/web/node_modules/.bin/playwright install chromium" >&2
    return 2
  fi
  release_build
  python3 tools/web/server.py --port "$port" --library "$OUT-release/libtts_text_frontend.so" \
    --resources resources > "${TMPDIR:-/tmp}/ttf-ci-web.log" 2>&1 &
  pid=$!
  # EXIT, not RETURN: errexit leaves the shell without running RETURN traps.
  trap 'kill "$pid" 2>/dev/null || true' EXIT
  for _ in $(seq 1 30); do
    curl --fail --silent "http://127.0.0.1:$port/api/health" >/dev/null && break
    sleep 1
  done
  node tests/web/browser.cjs "http://127.0.0.1:$port"
}

run() {
  local job=$1 start
  start=$(date +%s)
  echo "::group::$job"
  case "$job" in
    gcc) job_gcc ;;
    clang) job_clang ;;
    sanitizers) job_sanitizers ;;
    text-evaluation) job_text_evaluation ;;
    web-browser) job_web_browser ;;
    *) echo "unknown job: $job" >&2; exit 64 ;;
  esac
  echo "::endgroup::"
  echo "== $job passed in $(( $(date +%s) - start ))s"
}

case "${1:-}" in
  quick) for job in gcc text-evaluation; do run "$job"; done ;;
  full) for job in gcc clang sanitizers text-evaluation; do run "$job"; done ;;
  gcc | clang | sanitizers | text-evaluation | web-browser) run "$1" ;;
  *)
    sed -n '2,11p' "$0" >&2
    exit 64
    ;;
esac
