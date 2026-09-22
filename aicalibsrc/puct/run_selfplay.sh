#!/usr/bin/env bash
# run_selfplay.sh -- A14 AlphaOracle Prime Plus I Stage 2 self-play corpus
# generation launcher. Ported from aicalibsrc/ismctsnn/run_selfplay.sh --
# see that file for the full design rationale (process-level parallelism,
# wall-clock-bounded via `timeout` so the same script serves a pilot and
# the full run, seed_ledger.tsv's no-seed-reuse guarantee, the CPU/corpus-
# size monitor). Only the target binary, record size, and default label
# prefix differ -- the corpus format is the full-legal-move-list format
# Stage 4's policy head needs (aicalibsrc/puct/gen_policy_corpus.c), not
# A11's own state+outcome format -- the two corpora are not interchangeable
# even when the teacher is the same agent.
#
# Teacher generalized 2026-09-22 (A16 Session 2 item 2) -- previously
# hardcoded to A11 (ismctsnn). `puct` teaches from A14 in whatever
# PUCTParams its module defaults currently are; as of 2026-09-22 that's
# use_puct=false, i.e. today's real shipped A14 config, not a forced
# PUCT-selection variant -- see gen_policy_corpus.c's own Usage comment.
#
# Usage:
#   ./run_selfplay.sh <label> <duration_seconds> <teacher> [weights_path] [workers] [limit_iterations] [matchups_csv]
#
# Examples:
#   ./run_selfplay.sh pilot 3600 ismctsnn                    # 1-hour A11-taught pilot, auto worker count (~75% CPU)
#   ./run_selfplay.sh round2 43200 puct                      # 12-hour A14-taught run, once the pilot looks good
#   ./run_selfplay.sh smoketest 60 puct '' 4 200              # fast wiring check: 60s, 4 workers, tiny budget
#
# `teacher` is ismctsnn (A11) or puct (A14) -- required, no default, since
# silently generating against the wrong teacher would corrupt the corpus.
# `weights_path` defaults per-teacher (assets/ismctsnn/prime_657k_weights.bin
# or assets/puct/plus1_weights.bin) if omitted or passed as ''; pass your
# own to teach from a different checkpoint (e.g. a later bootstrap round).
# `workers` defaults to 75% of nproc, split round-robin across the opponent
# pool given by `matchups_csv` (default: mirror,vs_a7,vs_a3 -- A11's own
# original curated pool; vs_a4/vs_a6 are also supported, see
# gen_policy_corpus.c). `limit_iterations` defaults to 0, which tells
# gen_policy_corpus to use the teacher's own shipped default (4000 either
# way) -- only override it for a quick wiring smoke test, never for a real
# corpus (it must reflect the agent being distilled from). Every worker
# refuses to run without a successfully loaded weights file.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
GEN_CORPUS="$REPO_ROOT/bin/gen_policy_corpus"

# No longer needs cwd == repo root (removed 2026-09-22): gen_policy_corpus.c
# now takes an explicit weights_path argument instead of loading A11's
# weights from a fixed repo-root-relative path, and every other path this
# script builds ($GEN_CORPUS, $CORPUS_DIR, $LEDGER, $LOG_DIR) is already
# absolute via $SCRIPT_DIR/$REPO_ROOT. A relative --weights_path the CALLER
# passes is left relative to wherever they invoked this script from, not
# silently reinterpreted against the repo root.
CORPUS_DIR="$SCRIPT_DIR/corpus"
LEDGER="$CORPUS_DIR/seed_ledger.tsv"
LOG_DIR="$CORPUS_DIR/logs"
RECORD_BYTES=6768 # (537 + 1 + 1 + 1 + 128*9) * sizeof(float) -- bumped
                   # 2026-09-22 for the total_visits field added that date;
                   # see gen_policy_corpus.c's own record format note.
                   # Pre-2026-09-22 shards are 6764 bytes/record (no
                   # total_visits) -- this script only ever reports on
                   # shards it just generated itself (this run's own
                   # $LABEL), so the new width is always the right one here
MONITOR_INTERVAL_S=600 # 10 minutes

USAGE="Usage: $0 <label> <duration_seconds> <teacher: ismctsnn|puct> [weights_path] [workers] [limit_iterations] [matchups_csv]"
LABEL="${1:?$USAGE}"
DURATION="${2:?$USAGE}"
TEACHER="${3:?$USAGE}"
case "$TEACHER" in
  ismctsnn) DEFAULT_WEIGHTS="$REPO_ROOT/assets/ismctsnn/prime_657k_weights.bin" ;;
  puct)     DEFAULT_WEIGHTS="$REPO_ROOT/assets/puct/plus1_weights.bin" ;;
  *) echo "teacher must be one of: ismctsnn, puct" >&2; exit 1 ;;
esac
WEIGHTS_PATH="${4:-$DEFAULT_WEIGHTS}"
[ -n "$WEIGHTS_PATH" ] || WEIGHTS_PATH="$DEFAULT_WEIGHTS" # '' also falls back, see Usage note
WORKERS="${5:-$(( $(nproc) * 3 / 4 ))}"
LIMIT_ITERATIONS="${6:-0}"
NUMGAMES_CAP=10000000 # effectively unbounded; `timeout` is the real limit

IFS=',' read -r -a MATCHUPS <<< "${7:-mirror,vs_a7,vs_a3}"
NUM_MATCHUPS=${#MATCHUPS[@]}

if [ ! -x "$GEN_CORPUS" ]; then
  echo "bin/gen_policy_corpus not built -- run 'make gen_policy_corpus' from the repo root first" >&2
  exit 1
fi

mkdir -p "$CORPUS_DIR" "$LOG_DIR"
if [ ! -s "$LEDGER" ]; then
  # `teacher` appended 2026-09-22 as the LAST column (not inserted earlier)
  # so pre-existing rows (fewer columns, teacher implicitly ismctsnn --
  # A11 was the only teacher before that date) stay valid for next_seed()'s
  # own field-1-only read; nothing else in this script indexes by a fixed
  # later column.
  printf 'seed\tlabel\tmatchup\tworker\ttimestamp\toutput_path\tteacher\n' > "$LEDGER"
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
echo "=== A14 self-play (teacher: $TEACHER, weights: $WEIGHTS_PATH): label=$LABEL duration=${DURATION}s workers=$WORKERS limit_iterations=${LIMIT_ITERATIONS} ==="
echo "Started: $(date -Iseconds)"

pids=()
for ((i = 0; i < WORKERS; i++)); do
  matchup="${MATCHUPS[$((i % NUM_MATCHUPS))]}"
  seed=$(next_seed)
  outfile="$CORPUS_DIR/${LABEL}_${matchup}_seed${seed}.bin"
  logfile="$LOG_DIR/${LABEL}_${matchup}_seed${seed}.log"
  printf '%s\t%s\t%s\t%d\t%s\t%s\t%s\n' "$seed" "$LABEL" "$matchup" "$i" "$(date -Iseconds)" \
    "$outfile" "$TEACHER" >> "$LEDGER"

  timeout "$DURATION" "$GEN_CORPUS" "$TEACHER" "$WEIGHTS_PATH" "$matchup" "$NUMGAMES_CAP" "$seed" \
    "$outfile" "$LIMIT_ITERATIONS" > "$logfile" 2>&1 &
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
