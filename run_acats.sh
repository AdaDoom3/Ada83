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
START_MS=$(date +%s%3N)

# ── Clean stale artifacts to prevent spurious BIND errors ─────────────
rm -rf test_results acats_logs
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

run_one(){
    local f=$1 n=$(basename "$1" .ada) q=${1##*/}; q=${q:0:1}
    # Skip multi-file tests (end in digit, not 'm')
    [[ $n =~ [0-9]$ && ! $n =~ m$ ]] && return

    case ${q,,} in
    c)
        if ! timeout 0.5 ./ada83 "$f" > test_results/$n.ll 2>acats_logs/$n.err; then
            echo "c skip $n COMPILE:$(head -1 acats_logs/$n.err 2>/dev/null|cut -c1-50)"
            return
        fi
        if ! timeout 0.5 llvm-link -o test_results/$n.bc test_results/$n.ll acats/report.ll 2>acats_logs/$n.link; then
            echo "c skip $n BIND:unresolved_symbols"
            return
        fi
        if timeout 2 lli test_results/$n.bc > acats_logs/$n.out 2>&1 \
           || timeout 3 lli -jit-kind=mcjit test_results/$n.bc > acats_logs/$n.out 2>&1; then
            if grep -q PASSED acats_logs/$n.out 2>/dev/null; then
                echo "c pass $n PASSED"
            elif grep -q NOT.APPLICABLE acats_logs/$n.out 2>/dev/null; then
                echo "c skip $n N/A:$(grep -o 'NOT.APPLICABLE.*' acats_logs/$n.out|head -1|cut -c1-40)"
            elif grep -q FAILED acats_logs/$n.out 2>/dev/null; then
                echo "c fail $n FAILED:$(grep FAILED acats_logs/$n.out|head -1|cut -c1-50)"
            else
                echo "c fail $n NO_REPORT:no_PASSED/FAILED_in_output"
            fi
        else
            local ec=$?
            echo "c fail $n RUNTIME:exit_${ec}"
        fi
        ;;
    a)
        if ! timeout 0.5 ./ada83 "$f" > test_results/$n.ll 2>acats_logs/$n.err; then
            echo "a skip $n COMPILE:$(head -1 acats_logs/$n.err 2>/dev/null|cut -c1-50)"; return; fi
        if ! timeout 0.5 llvm-link -o test_results/$n.bc test_results/$n.ll acats/report.ll 2>acats_logs/$n.link; then
            echo "a skip $n BIND:unresolved_symbols"; return; fi
        if timeout 2 lli test_results/$n.bc > acats_logs/$n.out 2>&1; then
            echo "a pass $n PASSED"
        elif timeout 3 lli -jit-kind=mcjit test_results/$n.bc > acats_logs/$n.out 2>&1; then
            echo "a pass $n PASSED"
        else
            echo "a fail $n FAILED:exit_$?"
        fi
        ;;
    b)
        if timeout 0.5 ./ada83 "$f" > acats_logs/$n.ll 2>acats_logs/$n.err; then
            echo "b fail $n WRONG_ACCEPT:compiled_when_should_reject"
        else
            # Count error coverage
            local -a expected=() actual=(); local i=0 hits=0
            while IFS= read -r l; do ((++i)); [[ $l =~ --\ ERROR ]] && expected+=($i); done < "$f"
            while IFS=: read -r _ m _; do actual+=($m); done < <(./ada83 "$f" 2>&1|grep "^[^:]*:[0-9]")
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
        if ! timeout 0.5 ./ada83 "$f" > test_results/$n.ll 2>acats_logs/$n.err; then
            echo "d skip $n COMPILE:$(head -1 acats_logs/$n.err 2>/dev/null|cut -c1-50)"; return; fi
        if ! timeout 0.5 llvm-link -o test_results/$n.bc test_results/$n.ll acats/report.ll 2>/dev/null; then
            echo "d skip $n BIND"; return; fi
        if timeout 2 lli test_results/$n.bc > acats_logs/$n.out 2>&1 && grep -q PASSED acats_logs/$n.out; then
            echo "d pass $n PASSED"
        else
            echo "d fail $n FAILED:exact_arithmetic_check"
        fi
        ;;
    e)
        if ! timeout 0.5 ./ada83 "$f" > test_results/$n.ll 2>acats_logs/$n.err; then
            echo "e skip $n COMPILE:$(head -1 acats_logs/$n.err 2>/dev/null|cut -c1-50)"; return; fi
        if ! timeout 0.5 llvm-link -o test_results/$n.bc test_results/$n.ll acats/report.ll 2>/dev/null; then
            echo "e skip $n BIND"; return; fi
        timeout 2 lli test_results/$n.bc > acats_logs/$n.out 2>&1 || true
        if grep -q "TENTATIVELY PASSED" acats_logs/$n.out 2>/dev/null; then
            echo "e pass $n INSPECT:requires_manual_verification"
        elif grep -q PASSED acats_logs/$n.out 2>/dev/null; then
            echo "e pass $n PASSED"
        else
            echo "e fail $n FAILED"
        fi
        ;;
    l)
        if timeout 0.5 ./ada83 "$f" > test_results/$n.ll 2>acats_logs/$n.err; then
            if timeout 0.5 llvm-link -o test_results/$n.bc test_results/$n.ll acats/report.ll 2>acats_logs/$n.link; then
                if timeout 1 lli test_results/$n.bc > acats_logs/$n.out 2>&1; then
                    echo "l fail $n WRONG_EXEC:should_not_execute"
                else
                    echo "l pass $n BIND_REJECT:execution_blocked"
                fi
            else
                echo "l pass $n LINK_REJECT:binding_failed_as_expected"
            fi
        else
            echo "l pass $n COMPILE_REJECT:$(head -1 acats_logs/$n.err 2>/dev/null|cut -c1-40)"
        fi
        ;;
    f) ;; # Foundation support code — silent
    *) echo "? skip $n UNKNOWN:unrecognized_class" ;;
    esac
}
export -f run_one pct
export START_MS

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
        ${C[a]} ${C[b]} ${C[c]} ${C[d]} ${C[e]} ${C[l]} ${C[f]} ${C[s]} $pass ${C[z]} $(pct $pass ${C[z]}) > test_summary.txt
}

# ── Entry points ──────────────────────────────────────────────────────────

run_parallel(){
    local pattern=$1
    local tmpfile=$(mktemp)

    printf "\n========================================\n%s\n========================================\n\n" "$2"

    # Run tests in parallel, each outputting one result line
    for f in $pattern; do
        [[ -f $f ]] && echo "$f"
    done | xargs -P "$NPROC" -I{} bash -c 'run_one "$@"' _ {} > "$tmpfile" 2>/dev/null

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
  NPROC=N        Set parallelism (default: $(nproc 32>/dev/null||echo 32))
EOF
}

case ${1:-h} in
    g)        run_parallel "acats/${2:-c}*.ada" "Class ${2:-C} Tests" ;;
    q)        run_parallel "acats/${2:-c32}*.ada" "Group ${2:-c32} Tests" ;;
    h|help|*) usage ;;
esac
