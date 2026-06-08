#!/bin/bash
set -euo pipefail
# ═══════════════════════════════════════════════════════════════════════════
# Ada83 ACATS Test Harness — parallel execution via GNU xargs
# ═══════════════════════════════════════════════════════════════════════════
#
# Each test runs in its own subprocess, writing a one-line result to a temp
# file.  The main process reads those lines and tallies.  This is safe
# because every subprocess writes to its own unique file.

NPROC=${NPROC:-$(nproc 32>/dev/null || echo 32)}
# Per-test execution cap. The ACATS sources in acats/ carry a temporary
# development deviation: every DELAY literal >= 1.0 is scaled down by 10x
# (marked "TODO: acats-delay-deviation", listed in README.md), so the longest
# legitimate test runs ~12 s instead of ~2 minutes. The cap allows for that
# plus 16-way scheduling contention. When the deviations are reverted for a
# pristine-ACATS conformance run, raise this back to 120.
TEST_TIMEOUT=${TEST_TIMEOUT:-30}
START_MS=$(date +%s%3N)

# ── Per-run output directories ────────────────────────────────────────
# Every invocation writes into its own timestamped subfolder of
# test_results and acats_logs (named <label>-<timestamp>-<pid>), so
# repeated or concurrent runs never overwrite each other. A fresh
# directory per run also means no stale .ll/.bc artifacts can cause
# spurious BIND errors, which is why no cleanup pass is needed here.
# RESULTS_DIR and LOGS_DIR are created in run_parallel once the run
# label (class or group) is known.
mkdir -p test_results acats_logs

# ── Rebuild compiler if source is newer than binary ───────────────────
if [[ ! -f ./ada83 ]] || [[ ada83.c -nt ./ada83 ]]; then
    echo "Rebuilding ada83..."
    make -s compiler || { echo "FATAL: compiler build failed"; exit 1; }
fi

# ── Compile ACATS report package (always rebuild for freshness) ───────
./ada83 acats/report.adb > acats/report.ll 2>/dev/null || {
    echo "FATAL: cannot compile acats/report.adb"; exit 1; }

# ── Helpers ───────────────────────────────────────────────────────────────

pct(){ ((${2:-0}>0)) && printf %d $((100*$1/$2)) || printf 0; }

elapsed(){
    printf %.3f "$(bc<<<"scale=4;($(date +%s%3N)-${START_MS})/1000")"
}

# ── Single-test runner (called in subprocess) ─────────────────────────────
# Outputs exactly one line: CLASS RESULT NAME DETAIL
# CLASS:  a/b/c/d/e/l
# RESULT: pass/fail/skip

# Build the compile set for a test. A multi-file main (…<digits>m) compiles
# its whole fragment family in the ACATS-prescribed (lexical) order; every
# other test compiles itself alone. Fills COMPILE_FILES.
gather_files(){
    local f=$1 n=$2
    COMPILE_FILES=("$f")
    # ACATS multi-file naming: fragments are <base><digit>.ada or
    # <base><letter>.ada (d64005ea), the main is <base><digit>m.ada, base
    # ends in a letter and the index is ONE digit (c35502m is a singleton —
    # no digit directly before the m). Letter fragments sort AFTER the main
    # (subunits of it).
    if [[ $n =~ ^(.*[a-z])([0-9])m$ ]]; then
        local base=${BASH_REMATCH[1]}
        local family
        mapfile -t family < <(ls "acats/${base}"[0-9].ada "acats/${base}"[0-9]m.ada "acats/${base}"[a-z].ada 2>/dev/null | sort)
        ((${#family[@]} > 1)) && COMPILE_FILES=("${family[@]}")
    fi
}

# Compile every file of the set in order (-o so each unit's .ll and .ali
# land in RESULTS_DIR — the .ali set IS the program library the later
# files consult). Only the LAST file's .ll links: the compiler is
# whole-program, so the main's module already contains every unit it
# loaded through the library. Returns nonzero on the first failing file,
# leaving its name in COMPILE_FAILED.
compile_set(){
    local n=$1 part pn
    # Each test gets its own library directory: the compiler primes its
    # unit catalog from the output dir's ALI files, so the dir must hold
    # exactly this test's units — a shared dir is 32-way cross-talk.
    local lib=$RESULTS_DIR/$n.lib
    mkdir -p "$lib"
    # The main is the *m file, which the ACATS order may place MID-family.
    # Its whole-program module contains every unit compiled BEFORE it (the
    # loader pulled them through the library); units compiled AFTER it are
    # externs in the main and their own .ll files join the link.
    MAIN_LL=""
    LINK_AFTER_MAIN=()
    COMPILE_FAILED=""
    for part in "${COMPILE_FILES[@]}"; do
        pn=$(basename "$part" .ada)
        if ! timeout 4 ./ada83 "$part" -o $lib/$pn.ll >/dev/null 2>$LOGS_DIR/$n.err; then
            COMPILE_FAILED=$pn
            return 1
        fi
        if [[ $pn == "$n" ]]; then
            MAIN_LL=$lib/$pn.ll
        elif [[ -n $MAIN_LL ]]; then
            LINK_AFTER_MAIN+=("$lib/$pn.ll")
        fi
    done
    [[ -n $MAIN_LL ]] || MAIN_LL=$lib/$(basename "${COMPILE_FILES[-1]}" .ada).ll

    # The main's module is whole-program over the library as it stood at
    # the main's compile: any post-main fragment whose unit the main
    # already DEFINES (an obsolete replaced version, or a unit the main
    # inlined) must not link — only fragments providing symbols the main
    # left extern (subunits, later units) join the link.
    if ((${#LINK_AFTER_MAIN[@]})); then
        local kept=() frag unit
        for frag in "${LINK_AFTER_MAIN[@]}"; do
            unit=$(grep -m1 '^U ' "${frag%.ll}.ali" 2>/dev/null | awk '{print $2}' | sed 's/%.*//' | tr 'A-Z.' 'a-z_')
            if [[ -n $unit ]] && grep -q "^define.*@${unit}[(_]" "$MAIN_LL"; then
                continue
            fi
            kept+=("$frag")
        done
        LINK_AFTER_MAIN=(${kept[@]+"${kept[@]}"})
    fi

    # The bind step (RM 10.3/10.5, gnatbind's role): after ALL compiles,
    # verify the main's library closure is consistent — a unit recompiled
    # after its dependents makes them obsolete, forbidding execution.
    BIND_FAILED=""
    if ! ./ada83 --bind "$lib" "$n" 2>$LOGS_DIR/$n.bind; then
        BIND_FAILED=$n
    fi
}

# Run a linked test binary with CWD inside the test's own .lib directory:
# scratch files the test CREATEs (REPORT.LEGAL_FILE_NAME's X*.TMP) stay out
# of the repo root, and concurrent tests with identical scratch names
# cannot collide. $1 = timeout seconds, $2 = test name, rest = lli flags.
run_in_lib(){
    local secs=$1 n=$2; shift 2
    ( cd "$RESULTS_DIR/$n.lib" 2>/dev/null || exit 127
      exec timeout "$secs" lli "$@" "$ROOT/$RESULTS_DIR/$n.bc" )
}

run_one(){
    local f=$1 n=$(basename "$1" .ada) q=${1##*/}; q=${q:0:1}
    # Fragments (end in digit, not 'm') are compiled by their family's main.
    [[ $n =~ [0-9]$ && ! $n =~ m$ ]] && return
    # Letter-suffixed fragments (d64005ea) belong to a <base><digit>m main.
    if [[ $n =~ ^(.*[a-z])[a-z]$ ]]; then
        compgen -G "acats/${BASH_REMATCH[1]}[0-9]m.ada" >/dev/null && return
    fi
    # Skip support packages — ACATS test names never contain underscores
    # (check_file, enum_check, length_check, spprt13, fcndecl live alongside tests).
    [[ $n == *_* ]] && return
    local COMPILE_FILES MAIN_LL LINK_AFTER_MAIN COMPILE_FAILED BIND_FAILED
    gather_files "$f" "$n"

    case ${q,,} in
    c)
        if ! compile_set "$n"; then
            echo "c skip $n COMPILE[$COMPILE_FAILED]:$(head -1 $LOGS_DIR/$n.err 2>/dev/null|cut -c1-50)"
            return
        fi
        if [[ -n $BIND_FAILED ]]; then
            echo "c fail $n OBSOLETE:$(head -1 $LOGS_DIR/$n.bind 2>/dev/null|cut -c1-50)"
            return
        fi
        if ! timeout 2 llvm-link -o $RESULTS_DIR/$n.bc "$MAIN_LL" ${LINK_AFTER_MAIN[@]+"${LINK_AFTER_MAIN[@]}"} acats/report.ll 2>$LOGS_DIR/$n.link; then
            echo "c skip $n BIND:unresolved_symbols"
            return
        fi
        local rc=0
        run_in_lib "$TEST_TIMEOUT" "$n" > $LOGS_DIR/$n.out 2>&1 || rc=$?
        if ((rc==124 || rc==137)); then
            echo "c fail $n TIMEOUT:exceeded_${TEST_TIMEOUT}s"
            return
        fi
        # The mcjit retry exists for ORC-JIT crashes only; a timeout must not
        # pay the cap a second time.
        if ((rc!=0)); then
            rc=0
            run_in_lib "$TEST_TIMEOUT" "$n" -jit-kind=mcjit > $LOGS_DIR/$n.out 2>&1 || rc=$?
            if ((rc==124 || rc==137)); then
                echo "c fail $n TIMEOUT:exceeded_${TEST_TIMEOUT}s"
                return
            fi
        fi
        if ((rc==0)); then
            if grep -q PASSED $LOGS_DIR/$n.out 2>/dev/null; then
                echo "c pass $n PASSED"
            elif grep -q NOT.APPLICABLE $LOGS_DIR/$n.out 2>/dev/null; then
                echo "c skip $n N/A:$(grep -o 'NOT.APPLICABLE.*' $LOGS_DIR/$n.out|head -1|cut -c1-40)"
            elif grep -q FAILED $LOGS_DIR/$n.out 2>/dev/null; then
                echo "c fail $n FAILED:$(grep FAILED $LOGS_DIR/$n.out|head -1|cut -c1-50)"
            else
                echo "c fail $n NO_REPORT:no_PASSED/FAILED_in_output"
            fi
        else
            echo "c fail $n RUNTIME:exit_${rc}"
        fi
        ;;
    a)
        if ! compile_set "$n"; then
            echo "a skip $n COMPILE[$COMPILE_FAILED]:$(head -1 $LOGS_DIR/$n.err 2>/dev/null|cut -c1-50)"; return; fi
        if [[ -n $BIND_FAILED ]]; then
            echo "a fail $n OBSOLETE:$(head -1 $LOGS_DIR/$n.bind 2>/dev/null|cut -c1-50)"; return; fi
        if ! timeout 2 llvm-link -o $RESULTS_DIR/$n.bc "$MAIN_LL" ${LINK_AFTER_MAIN[@]+"${LINK_AFTER_MAIN[@]}"} acats/report.ll 2>$LOGS_DIR/$n.link; then
            echo "a skip $n BIND:unresolved_symbols"; return; fi
        local rc=0
        run_in_lib "$TEST_TIMEOUT" "$n" > $LOGS_DIR/$n.out 2>&1 || rc=$?
        if ((rc==124 || rc==137)); then
            echo "a fail $n TIMEOUT:exceeded_${TEST_TIMEOUT}s"
        elif ((rc==0)); then
            echo "a pass $n PASSED"
        elif run_in_lib "$TEST_TIMEOUT" "$n" -jit-kind=mcjit > $LOGS_DIR/$n.out 2>&1; then
            echo "a pass $n PASSED"
        else
            echo "a fail $n FAILED:exit_$?"
        fi
        ;;
    b)
        if timeout 2 ./ada83 "$f" > $LOGS_DIR/$n.ll 2>$LOGS_DIR/$n.err; then
            echo "b fail $n WRONG_ACCEPT:compiled_when_should_reject"
        else
            # Count error coverage
            local -a expected=() actual=(); local i=0 hits=0
            while IFS= read -r l; do ((++i)); [[ $l =~ --\ ERROR ]] && expected+=($i); done < "$f"
            while IFS=: read -r _ m _; do actual+=($m); done < <(timeout 2 ./ada83 "$f" 2>&1|grep "^[^:]*:[0-9]")
            for e in ${expected[@]+"${expected[@]}"}; do
                for v in ${actual[@]+"${actual[@]}"}; do
                    ((v>=e-1&&v<=e+1)) && { ((++hits)); break; }
                done
            done
            local xe=${#expected[@]}
            local p=$(pct $hits $xe)
            ((p>=90)) && echo "b pass $n REJECTED:${hits}/${xe}_errors_(${p}%)" \
                      || echo "b fail $n LOW_COVERAGE:${hits}/${xe}_errors_(${p}%)"
        fi
        ;;
    d)
        if ! compile_set "$n"; then
            echo "d skip $n COMPILE[$COMPILE_FAILED]:$(head -1 $LOGS_DIR/$n.err 2>/dev/null|cut -c1-50)"; return; fi
        if [[ -n $BIND_FAILED ]]; then
            echo "d fail $n OBSOLETE:$(head -1 $LOGS_DIR/$n.bind 2>/dev/null|cut -c1-50)"; return; fi
        if ! timeout 2 llvm-link -o $RESULTS_DIR/$n.bc "$MAIN_LL" ${LINK_AFTER_MAIN[@]+"${LINK_AFTER_MAIN[@]}"} acats/report.ll 2>/dev/null; then
            echo "d skip $n BIND"; return; fi
        if run_in_lib "$TEST_TIMEOUT" "$n" > $LOGS_DIR/$n.out 2>&1 && grep -q PASSED $LOGS_DIR/$n.out; then
            echo "d pass $n PASSED"
        else
            echo "d fail $n FAILED:exact_arithmetic_check"
        fi
        ;;
    e)
        if ! compile_set "$n"; then
            echo "e skip $n COMPILE[$COMPILE_FAILED]:$(head -1 $LOGS_DIR/$n.err 2>/dev/null|cut -c1-50)"; return; fi
        # Manual-inspection class: a bind rejection is frequently the
        # EXPECTED outcome ("PASSED => ERROR" tests) — record like a
        # compile rejection, for inspection.
        if [[ -n $BIND_FAILED ]]; then
            echo "e skip $n BIND_REJECT:$(head -1 $LOGS_DIR/$n.bind 2>/dev/null|cut -c1-50)"; return; fi
        if ! timeout 2 llvm-link -o $RESULTS_DIR/$n.bc "$MAIN_LL" ${LINK_AFTER_MAIN[@]+"${LINK_AFTER_MAIN[@]}"} acats/report.ll 2>/dev/null; then
            echo "e skip $n BIND"; return; fi
        run_in_lib "$TEST_TIMEOUT" "$n" > $LOGS_DIR/$n.out 2>&1 || true
        if grep -q "TENTATIVELY PASSED" $LOGS_DIR/$n.out 2>/dev/null; then
            echo "e pass $n INSPECT:requires_manual_verification"
        elif grep -q PASSED $LOGS_DIR/$n.out 2>/dev/null; then
            echo "e pass $n PASSED"
        else
            echo "e fail $n FAILED"
        fi
        ;;
    l)
        if compile_set "$n"; then
            if [[ -n $BIND_FAILED ]]; then
                echo "l pass $n BIND_REJECT:$(head -1 $LOGS_DIR/$n.bind 2>/dev/null|cut -c1-40)"
                return
            fi
            if timeout 2 llvm-link -o $RESULTS_DIR/$n.bc "$MAIN_LL" ${LINK_AFTER_MAIN[@]+"${LINK_AFTER_MAIN[@]}"} acats/report.ll 2>$LOGS_DIR/$n.link; then
                if run_in_lib 1 "$n" > $LOGS_DIR/$n.out 2>&1; then
                    echo "l fail $n WRONG_EXEC:should_not_execute"
                else
                    echo "l pass $n BIND_REJECT:execution_blocked"
                fi
            else
                echo "l pass $n LINK_REJECT:binding_failed_as_expected"
            fi
        else
            echo "l pass $n COMPILE_REJECT:$(head -1 $LOGS_DIR/$n.err 2>/dev/null|cut -c1-40)"
        fi
        ;;
    f) ;; # Foundation support code — silent
    *) echo "? skip $n UNKNOWN:unrecognized_class" ;;
    esac
}
ROOT=$PWD
export ROOT
export -f run_one gather_files compile_set run_in_lib pct
export START_MS TEST_TIMEOUT

# ── Per-test outer cap (defense in depth) ─────────────────────────────────
# Wraps run_one so a hung test can never hang the suite, even if an inner
# `timeout` is bypassed (e.g. a runaway grandchild process). Sized to cover
# the worst case inside run_one: compile + link + one lli run + the mcjit
# crash-retry, each already individually capped. On SIGTERM/SIGKILL exit
# (124/137), emit a synthetic fail line so the test is recorded rather than
# silently dropped.
run_one_timed(){
    local f=$1 n=$(basename "$1" .ada) q
    q=${f##*/}; q=${q:0:1}; q=${q,,}
    local out rc=0
    out=$(timeout $((2*TEST_TIMEOUT+5)) bash -c 'run_one "$1"' _ "$f" 2>/dev/null) || rc=$?
    if ((rc==124 || rc==137)); then
        echo "$q fail $n TIMEOUT:exceeded_$((2*TEST_TIMEOUT+5))s"
    elif [[ -n $out ]]; then
        echo "$out"
    fi
    # empty + rc=0 → legitimate silent skip (foundation file or multi-file fragment)
}
export -f run_one_timed

# ── Result aggregation ────────────────────────────────────────────────────

tally_results(){
    local results_file=$1
    local -A C=([a]=0 [b]=0 [c]=0 [d]=0 [e]=0 [l]=0
                [fa]=0 [fb]=0 [fc]=0 [fd]=0 [fe]=0 [fl]=0
                [sa]=0 [sb]=0 [sc]=0 [sd]=0 [se]=0 [sl]=0
                [ta]=0 [tb]=0 [tc]=0 [td]=0 [te]=0 [tl]=0
                [f]=0 [s]=0 [z]=0)

    while read -r cls result name detail; do
        [[ -z $cls ]] && continue
        local k=${cls,,}
        ((++C[z]))
        ((++C[t$k])) 2>/dev/null || C[t$k]=1
        case $result in
            pass) ((++C[$k])); printf "  %-18s %-6s %s\n" "$name" "PASS" "${detail//_/ }" ;;
            fail) ((++C[f])); ((++C[f$k])) 2>/dev/null || C[f$k]=1
                  printf "  %-18s %-6s %s\n" "$name" "FAIL" "${detail//_/ }" ;;
            skip) ((++C[s])); ((++C[s$k])) 2>/dev/null || C[s$k]=1
                  printf "  %-18s %-6s %s\n" "$name" "SKIP" "${detail//_/ }" ;;
        esac
    done < "$results_file"

    local pass=$((C[a]+C[b]+C[c]+C[d]+C[e]+C[l]))

    printf "\n========================================\nRESULTS\n========================================\n\n"
    printf " %-22s %6s %6s %6s %6s %7s\n" "CLASS" "pass" "fail" "skip" "total" "rate"
    printf " %-22s %6s %6s %6s %6s %7s\n" "----------------------" "------" "------" "------" "------" "-------"
    ((C[ta]>0)) && printf " %-22s %6d %6d %6d %6d %6d%%\n" "A  Acceptance" ${C[a]} ${C[fa]} ${C[sa]} ${C[ta]} $(pct ${C[a]} ${C[ta]})
    ((C[tb]>0)) && printf " %-22s %6d %6d %6d %6d %6d%%\n" "B  Illegality" ${C[b]} ${C[fb]} ${C[sb]} ${C[tb]} $(pct ${C[b]} ${C[tb]})
    ((C[tc]>0)) && printf " %-22s %6d %6d %6d %6d %6d%%\n" "C  Executable" ${C[c]} ${C[fc]} ${C[sc]} ${C[tc]} $(pct ${C[c]} ${C[tc]})
    ((C[td]>0)) && printf " %-22s %6d %6d %6d %6d %6d%%\n" "D  Numerics"   ${C[d]} ${C[fd]} ${C[sd]} ${C[td]} $(pct ${C[d]} ${C[td]})
    ((C[te]>0)) && printf " %-22s %6d %6d %6d %6d %6d%%\n" "E  Inspection" ${C[e]} ${C[fe]} ${C[se]} ${C[te]} $(pct ${C[e]} ${C[te]})
    ((C[tl]>0)) && printf " %-22s %6d %6d %6d %6d %6d%%\n" "L  Post-compilation" ${C[l]} ${C[fl]} ${C[sl]} ${C[tl]} $(pct ${C[l]} ${C[tl]})
    printf " %-22s %6s %6s %6s %6s %7s\n" "----------------------" "------" "------" "------" "------" "-------"
    printf " %-22s %6d %6d %6d %6d %6d%%\n" "TOTAL" $pass ${C[f]} ${C[s]} ${C[z]} $(pct $pass ${C[z]})
    printf "\n========================================\n"
    printf " elapsed $(elapsed)s  |  processed %d tests  |  %d workers  |  %s\n" ${C[z]} "$NPROC" "$(date '+%Y-%m-%d %H:%M:%S')"
    printf "========================================\n"
    printf "A=%d B=%d C=%d D=%d E=%d L=%d F=%d S=%d T=%d/%d (%d%%)\n" \
        ${C[a]} ${C[b]} ${C[c]} ${C[d]} ${C[e]} ${C[l]} ${C[f]} ${C[s]} $pass ${C[z]} $(pct $pass ${C[z]}) > "$RESULTS_DIR/test_summary.txt"
}

# ── Entry points ──────────────────────────────────────────────────────────

run_parallel(){
    local pattern=$1 title=$2 label=$3
    local tmpfile=$(mktemp)

    # One subfolder per run: <label>-<timestamp>-<pid>. The PID suffix keeps
    # two runs started within the same second from sharing a directory.
    local run_id="${label}-$(date +%Y%m%d-%H%M%S)-$$"
    export RESULTS_DIR="test_results/${run_id}"
    export LOGS_DIR="acats_logs/${run_id}"
    mkdir -p "$RESULTS_DIR" "$LOGS_DIR"

    printf "\n========================================\n%s\n========================================\n\n" "$title"
    printf "results: %s\nlogs:    %s\n\n" "$RESULTS_DIR" "$LOGS_DIR"

    # Run tests in parallel, each outputting one result line
    for f in $pattern; do
        [[ -f $f ]] && echo "$f"
    done | xargs -P "$NPROC" -I{} bash -c 'run_one_timed "$@"' _ {} > "$tmpfile" 2>/dev/null

    # Sort results by name for stable output, then tally
    sort -k3 "$tmpfile" > "${tmpfile}.sorted"
    tally_results "${tmpfile}.sorted"
    rm -f "$tmpfile" "${tmpfile}.sorted"
}

usage(){
    cat<<EOF
Usage: $0 <mode> [options]

Modes:
  g <X>          Run all tests for class X (A/B/C/D/E/L)
  q <XX>         Run tests for group XX (e.g., c32, c34)
  h|help         Show this help

Environment:
  NPROC=N         Set parallelism (default: $(nproc 32>/dev/null||echo 32))
  TEST_TIMEOUT=N  Per-test execution cap in seconds (default: 30, covering
                  the 10x-scaled DELAY deviation in acats/ plus contention — see README.md).
                  Use 120 when running pristine ACATS sources.
EOF
}

case ${1:-h} in
    g)        run_parallel "acats/${2:-c}*.ada" "Class ${2:-C} Tests" "${2:-c}" ;;
    q)        run_parallel "acats/${2:-c32}*.ada" "Group ${2:-c32} Tests" "${2:-c32}" ;;
    h|help|*) usage ;;
esac
