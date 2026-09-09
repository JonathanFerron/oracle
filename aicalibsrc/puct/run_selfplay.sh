#!/usr/bin/env bash
# run_selfplay.sh -- A14 AlphaOracle Prime Plus I Stage 2 self-play corpus
# generation launcher. Ported from aicalibsrc/ismctsnn/run_selfplay.sh --
# see that file for the full design rationale (process-level parallelism,
# wall-clock-bounded via `timeout` so the same script serves a pilot and
# the full run, seed_ledger.tsv's no-seed-reuse guarantee, the CPU/corpus-
# size monitor). Only the target binary, record size, and default label
# prefix differ -- the teacher here is A11 (ismctsnn), not A10, and the
# corpus format is the full-legal-move-list format Stage 4's policy head
# needs (aicalibsrc/puct/gen_policy_corpus.c), not A11's own state+outcome
# format -- the two corpora are not interchangeable despite the shared
# teacher.
#
# Usage:
#   ./run_selfplay.sh <label> <duration_seconds> [workers] [limit_iterations] [matchups_csv]
#
# Examples:
#   ./run_selfplay.sh pilot 3600                       # 1-hour pilot, auto worker count (~75% CPU)
#   ./run_selfplay.sh full 43200                       # 12-hour full run, once the pilot looks good
#   ./run_selfplay.sh smoketest 60 4 200               # fast wiring check: 60s, 4 workers, tiny budget
#
# `workers` defaults to 75% of nproc, split round-robin across the opponent
# pool given by `matchups_csv` (default: mirror,vs_a7,vs_a3 -- A11's own
# original curated pool; vs_a4/vs_a6 are also supported, see
# gen_policy_corpus.c). `limit_iterations` defaults to 0, which tells
# gen_policy_corpus to use A11's own shipped default (4000) -- only
# override it for a quick wiring smoke test, never for a real corpus (it
# must reflect the agent being distilled from). Requires a trained A11
# weights file at the fixed path gen_policy_corpus.c loads
# (assets/ismctsnn/prime_657k_weights.bin) -- every worker refuses to run
# without it.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
GEN_CORPUS="$REPO_ROOT/bin/gen_policy_corpus"

# gen_policy_corpus.c loads A11's weights from a repo-root-relative path
# (ISMCTSNN_DEFAULT_WEIGHTS_PATH, ai_strat_ismctsnn.h) -- unlike
# calib_ismctsnn.c, it takes no CLI override, so every worker needs cwd ==
# repo root regardless of where this script itself was invoked from.
cd "$REPO_ROOT"
CORPUS_DIR="$SCRIPT_DIR/corpus"
LEDGER="$CORPUS_DIR/seed_ledger.tsv"
LOG_DIR="$CORPUS_DIR/logs"
RECORD_BYTES=6764 # (537 + 1 + 1 + 128*9) * sizeof(float) -- see
                   # gen_policy_corpus.c's own record format note
MONITOR_INTERVAL_S=600 # 10 minutes

LABEL="${1:?Usage: $0 <label> <duration_seconds> [workers] [limit_iterations] [matchups_csv]}"
DURATION="${2:?Usage: $0 <label> <duration_seconds> [workers] [limit_iterations] [matchups_csv]}"
WORKERS="${3:-$(( $(nproc) * 3 / 4 ))}"
LIMIT_ITERATIONS="${4:-0}"
NUMGAMES_CAP=10000000 # effectively unbounded; `timeout` is the real limit

IFS=',' read -r -a MATCHUPS <<< "${5:-mirror,vs_a7,vs_a3}"
NUM_MATCHUPS=${#MATCHUPS[@]}

if [ ! -x "$GEN_CORPUS" ]; then
  echo "bin/gen_policy_corpus not built -- run 'make gen_policy_corpus' from the repo root first" >&2
  exit 1
fi

mkdir -p "$CORPUS_DIR" "$LOG_DIR"
if [ ! -s "$LEDGER" ]; then
  printf 'seed\tlabel\tmatchup\tworker\ttimestamp\toutput_path\n' > "$LEDGER"
fi
MONITOR_LOG="$LOG_DIR/${LABEL}_monitor.tsv"
printf 'timestamp\telapsed_s\tcpu_pct\tcorpus_mb_so_far\n' > "$MONITOR_LOG"

next_seed() {
  local last
  last=$(awk -F'\t' 'NR>1{print $1}' "$LEDGER" | sort -n | tail -1)
  echo $(( ${last:-0} + 1 ))
}

# System-wide CPU utilization over a 1s sample window, from /proc/stat --
# no external dependency (sysstat/mpstat) needed. This measures the whole
# machine, which is the right comparison against the 75%-CPU ceiling since
# these workers should dominate activity during a real run.
sample_cpu_pct() {
  local u1 n1 s1 i1 io1 irq1 si1 st1
  local u2 n2 s2 i2 io2 irq2 si2 st2
  read -r _ u1 n1 s1 i1 io1 irq1 si1 st1 _ < /proc/stat
  sleep 1
  read -r _ u2 n2 s2 i2 io2 irq2 si2 st2 _ < /proc/stat
  local idle1=$((i1 + io1)) idle2=$((i2 + io2))
  local total1=$((u1 + n1 + s1 + i1 + io1 + irq1 + si1 + st1))
  local total2=$((u2 + n2 + s2 + i2 + io2 + irq2 + si2 + st2))
  local totald=$((total2 - total1)) idled=$((idle2 - idle1))
  awk -v t="$totald" -v idl="$idled" 'BEGIN{ if(t<=0){print "0.0"} else {printf "%.1f", 100*(t-idl)/t} }'
}

any_worker_alive() {
  local pid
  for pid in "${pids[@]}"; do
    if kill -0 "$pid" 2>/dev/null; then
      return 0
    fi
  done
  return 1
}

monitor_workers() {
  local start_ts=$1
  while any_worker_alive; do
    sleep "$MONITOR_INTERVAL_S" &
    wait $! 2>/dev/null || true
    any_worker_alive || break
    local now elapsed cpu_pct bytes mb
    now=$(date +%s)
    elapsed=$((now - start_ts))
    cpu_pct=$(sample_cpu_pct)
    bytes=$(du -sb "$CORPUS_DIR" 2>/dev/null | awk '{print $1}')
    bytes=${bytes:-0}
    mb=$((bytes / 1024 / 1024))
    printf '%s\t%d\t%s\t%d\n' "$(date -Iseconds)" "$elapsed" "$cpu_pct" "$mb" | tee -a "$MONITOR_LOG"
  done
} # monitor_workers

START_TS=$(date +%s)
echo "=== A14 self-play (teacher: A11): label=$LABEL duration=${DURATION}s workers=$WORKERS limit_iterations=${LIMIT_ITERATIONS} ==="
echo "Started: $(date -Iseconds)"

pids=()
for ((i = 0; i < WORKERS; i++)); do
  matchup="${MATCHUPS[$((i % NUM_MATCHUPS))]}"
  seed=$(next_seed)
  outfile="$CORPUS_DIR/${LABEL}_${matchup}_seed${seed}.bin"
  logfile="$LOG_DIR/${LABEL}_${matchup}_seed${seed}.log"
  printf '%s\t%s\t%s\t%d\t%s\t%s\n' "$seed" "$LABEL" "$matchup" "$i" "$(date -Iseconds)" \
    "$outfile" >> "$LEDGER"

  timeout "$DURATION" "$GEN_CORPUS" "$matchup" "$NUMGAMES_CAP" "$seed" "$outfile" \
    "$LIMIT_ITERATIONS" > "$logfile" 2>&1 &
  pids+=($!)
  echo "  worker $i: matchup=$matchup seed=$seed -> $(basename "$outfile")"
done

echo "CPU/corpus-size monitor: every ${MONITOR_INTERVAL_S}s -> $MONITOR_LOG"
monitor_workers "$START_TS" &
MONITOR_PID=$!

echo "Waiting for all $WORKERS workers (up to ${DURATION}s each)..."
failures=0
for pid in "${pids[@]}"; do
  code=0
  wait "$pid" || code=$?
  # `timeout` exits 124 for its own kill -- that's the expected/normal way
  # every worker ends here (NUMGAMES_CAP is high enough gen_policy_corpus
  # never finishes on its own first). Only anything else (crash, bad usage,
  # a missing weights file, etc.) counts as a real failure worth
  # investigating in corpus/logs/.
  if [ "$code" -ne 124 ] && [ "$code" -ne 0 ]; then
    failures=$((failures + 1))
  fi
done
echo "($failures worker(s) exited with an unexpected code; check corpus/logs/ if that's > 0)"

kill "$MONITOR_PID" 2>/dev/null || true
wait "$MONITOR_PID" 2>/dev/null || true

END_TS=$(date +%s)
ELAPSED_S=$((END_TS - START_TS))

echo
echo "=== $LABEL run complete ==="
echo "Wall-clock elapsed: $(date -u -d "@$ELAPSED_S" +%H:%M:%S) (requested ${DURATION}s)"

total_bytes=0
shard_count=0
for f in "$CORPUS_DIR/${LABEL}"_*.bin; do
  [ -f "$f" ] || continue
  bytes=$(stat -c%s "$f")
  records=$((bytes / RECORD_BYTES))
  total_bytes=$((total_bytes + bytes))
  shard_count=$((shard_count + 1))
  printf '  %-55s %10d records  %8d MB\n' "$(basename "$f")" "$records" "$((bytes / 1024 / 1024))"
done

total_records=$((total_bytes / RECORD_BYTES))
total_mb=$((total_bytes / 1024 / 1024))
echo "  TOTAL: $total_records records, $total_mb MB ($(awk -v b="$total_bytes" 'BEGIN{printf "%.2f", b/1073741824}') GB), $shard_count shard files"

if [ "$ELAPSED_S" -gt 0 ]; then
  hourly_mb=$(awk -v mb="$total_mb" -v s="$ELAPSED_S" 'BEGIN{printf "%.0f", mb*3600/s}')
  hourly_records=$(awk -v r="$total_records" -v s="$ELAPSED_S" 'BEGIN{printf "%.0f", r*3600/s}')
  echo "  Throughput: ~${hourly_records} records/hour, ~${hourly_mb} MB/hour at $WORKERS workers"
  echo "  Extrapolated to a 12h run at the same worker count: ~$(( hourly_records * 12 )) records," \
       "~$(( hourly_mb * 12 / 1024 )) GB"
fi

echo "Seed ledger: $LEDGER"
echo "Per-worker logs + CPU/size monitor log: $LOG_DIR"
