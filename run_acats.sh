#!/usr/bin/env bash
set -euo pipefail
# ═══════════════════════════════════════════════════════════════════════════
# Ada83 ACATS conformance harness
# ═══════════════════════════════════════════════════════════════════════════
#
# Runs the ACATS suite in parallel with per-test isolation, and — the part that
# turns a pass-rate into a development signal — diffs each run against a
# committed baseline of expected outcomes. A run therefore reports NEW
# regressions and progressions, not just an absolute percentage, and `check`
# returns a nonzero exit status when anything regressed, so it gates CI.
#
# Usage:
#   run_acats.sh run   [SELECTOR]    run and print an absolute report
#   run_acats.sh check [SELECTOR]    run, diff vs baseline; exit 1 on any regression
#   run_acats.sh bless [SELECTOR]    run, then write the results as the new baseline
#   run_acats.sh list  [SELECTOR]    list the tests a selector expands to
#   run_acats.sh help
#
#   SELECTOR (default: all):
#     all              every acats/*.ada test (each classified by its own name)
#     a|b|c|d|e|l      one ACATS class
#     <prefix>         a filename prefix, e.g. c45, c452, c45347
#
# Compatibility: `g <class>` == `run <class>`, `q <group>` == `run <group>`.
#
# Environment:
#   JOBS / NPROC     parallel workers (default: nproc)
#   TEST_TIMEOUT     per-test run cap in seconds (default 30 — sized for the 10x
#                    DELAY deviation in acats/ plus contention; use 120 for
#                    pristine sources — see README.md)
#   BASELINE         baseline manifest path (default: acats.baseline)
#   TAP=1            also emit a TAP stream to <results>/results.tap
#
# A run always writes, into its own timestamped test_results/<id>/ directory:
#   results.tsv      machine-readable: name <TAB> class <TAB> status <TAB> detail
#   test_summary.txt one-line totals
#   results.tap      TAP stream (when TAP=1)

NPROC=${JOBS:-${NPROC:-$(nproc 2>/dev/null || echo 4)}}
TEST_TIMEOUT=${TEST_TIMEOUT:-30}
LINK_TIMEOUT=${LINK_TIMEOUT:-20}
BASELINE=${BASELINE:-acats.baseline}
START_MS=$(date +%s%3N)

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

# ═══════════════════════════════════════════════════════════════════════════
# EXECUTION CORE — one subprocess per test, each emitting exactly one line:
#   CLASS RESULT NAME DETAIL   (CLASS a/b/c/d/e/l ; RESULT pass/fail/skip ;
#   DETAIL a single _-joined token). This section encodes the ACATS run
# conventions (multi-file families, foundation files, the bind closure check,
# whole-program linking) and is deliberately kept intact.
# ═══════════════════════════════════════════════════════════════════════════

# Build the compile set for a test. A multi-file main (…<digits>m) compiles
# its whole fragment family in the ACATS-prescribed (lexical) order; every
# other test compiles itself alone. Fills COMPILE_FILES.
gather_files(){
    local f=$1 n=$2
    COMPILE_FILES=("$f")
    if [[ $n =~ ^(.*[a-z])([0-9])m$ ]]; then
        local base=${BASH_REMATCH[1]}
        local family
        mapfile -t family < <(ls "acats/${base}"[0-9].ada "acats/${base}"[0-9]m.ada "acats/${base}"[a-z].ada 2>/dev/null | sort)
        ((${#family[@]} > 1)) && COMPILE_FILES=("${family[@]}")
    fi
}

# Compile every file of the set in order (-o so each unit's .ll and .ali land
# in the test's library dir — the .ali set IS the program library the later
# files consult). Only the LAST/main file's .ll links (whole-program). Returns
# nonzero on the first failing file, leaving its name in COMPILE_FAILED, then
# runs the bind closure check (RM 10.3/10.5) leaving BIND_FAILED.
compile_set(){
    local n=$1 part pn
    local lib=$RESULTS_DIR/$n.lib
    mkdir -p "$lib"
    MAIN_LL=""
    LINK_FRAGMENTS=()
    COMPILE_FAILED=""
    for part in "${COMPILE_FILES[@]}"; do
        pn=$(basename "$part" .ada)
        if ! timeout 4 ./ada83 "$part" -o $lib/$pn.ll >/dev/null 2>$LOGS_DIR/$n.err; then
            # ACATS ships intentionally-erroneous fragments (ca3009a, …):
            # a rejected submission updates nothing and processing goes on.
            # Only the main unit failing to compile fails the set.
            if [[ $pn == "$n" ]]; then
                COMPILE_FAILED=$pn
                return 1
            fi
            continue
        fi
        if [[ $pn == "$n" ]]; then
            MAIN_LL=$lib/$pn.ll
        else
            LINK_FRAGMENTS+=("$lib/$pn.ll")
        fi
    done
    [[ -n $MAIN_LL ]] || MAIN_LL=$lib/$(basename "${COMPILE_FILES[-1]}" .ada).ll

    # A fragment module is linked only while it is the CURRENT provider of
    # some unit: it is dropped when the main module whole-loaded one of its
    # units (the compiler inlines WITH'd bodies), or when every unit it
    # provides was superseded — re-submitted by a later compilation, or a
    # subunit whose ancestor was recompiled (RM 10.1/10.3 replacement: the
    # later module owns the unit's code, the earlier one is obsolete).
    if ((${#LINK_FRAGMENTS[@]})); then
        local kept=() frag unit i p anc u_current
        local -a frag_units=()
        local -A last_provider=()
        for ((i = 0; i < ${#LINK_FRAGMENTS[@]}; i++)); do
            frag=${LINK_FRAGMENTS[i]}
            frag_units[i]=$(grep '^U ' "${frag%.ll}.ali" 2>/dev/null \
                            | awk '{print $2}' | sed 's/%.*//' | tr 'A-Z' 'a-z')
            for unit in ${frag_units[i]}; do last_provider[$unit]=$i; done
        done
        for ((i = 0; i < ${#LINK_FRAGMENTS[@]}; i++)); do
            frag=${LINK_FRAGMENTS[i]}
            local in_main=0 current=0
            [[ -n ${frag_units[i]//[$' \n']} ]] || current=1   # no ALI: keep
            for unit in ${frag_units[i]}; do
                u_current=1
                anc=$unit
                while :; do
                    p=${last_provider[$anc]:-$i}
                    ((p > i)) && u_current=0
                    [[ $anc == *.* ]] || break
                    anc=${anc%.*}
                done
                ((u_current)) && current=1
                # unit-owned symbols are @unit( or @unit__…; a single
                # underscore (@unit_s82) is an overload homograph, not
                # evidence the main whole-loaded this unit
                grep -Eq "^define.*@${unit//./__}(\(|__)" "$MAIN_LL" && in_main=1
            done
            ((in_main)) && continue
            ((current)) || continue
            kept+=("$frag")
        done
        LINK_FRAGMENTS=(${kept[@]+"${kept[@]}"})
    fi

    BIND_FAILED=""
    if ! ./ada83 --bind "$lib" "$n" 2>$LOGS_DIR/$n.bind; then
        BIND_FAILED=$n
    fi
}

# Run a linked test binary with CWD inside the test's own .lib directory, so
# scratch files it CREATEs stay out of the repo root and cannot collide across
# concurrent tests. $1 = timeout seconds, $2 = test name, rest = lli flags.
run_in_lib(){
    local secs=$1 n=$2; shift 2
    ( cd "$RESULTS_DIR/$n.lib" 2>/dev/null || exit 127
      exec timeout "$secs" lli "$@" "$ROOT/$RESULTS_DIR/$n.bc" )
}

# Whole-program link of MAIN_LL + surviving fragments + report.ll into $n.bc.
# llvm-link is CPU-trivial, but first-touch disk under N-way parallelism can
# blow a tight cap, so the timeout is generous; a kill (124/137) is reported as
# LINK_STATUS=timeout, kept distinct from a genuine unresolved-symbol error
# (LINK_STATUS=unresolved), so load artifacts can never masquerade as
# missing-runtime "skips". Returns llvm-link's own status.
link_program(){
    local n=$1 rc=0
    timeout "$LINK_TIMEOUT" llvm-link -o "$RESULTS_DIR/$n.bc" "$MAIN_LL" \
        ${LINK_FRAGMENTS[@]+"${LINK_FRAGMENTS[@]}"} acats/report.ll \
        2>"$LOGS_DIR/$n.link" || rc=$?
    case $rc in
        0)       LINK_STATUS=ok ;;
        124|137) LINK_STATUS=timeout ;;
        *)       LINK_STATUS=unresolved ;;
    esac
    return $rc
}

# ACATS Chapter-14 file continuity: a reader test (ce2108b, ce3112b, …) opens an
# external file left behind by a creator it names with LEGAL_FILE_NAME(_,
# "CExxx"). The ACATS model runs the creator first in the SAME directory; our
# per-test CWD isolation would otherwise hide the file. Compile, link, and run
# each named creator INTO THE READER'S OWN lib dir so its files are waiting when
# the reader opens them. Best-effort: a creator that will not build just leaves
# the reader to report the missing file as it would have without this step.
run_continuity_creators(){
    local reader=$1 lib=$2 self=${1,,} c
    for c in $(grep -oiE 'legal_file_name[ ]*\([^)]*"ce[0-9a-z]+"' "acats/$reader.ada" 2>/dev/null \
               | grep -oiE '"ce[0-9a-z]+"' | tr -d '"' | tr 'A-Z' 'a-z' | sort -u); do
        [[ $c == "$self" || ! -f acats/$c.ada ]] && continue
        ./ada83 "acats/$c.ada" -o "$lib/$c.ll" >/dev/null 2>&1 || continue
        timeout "$LINK_TIMEOUT" llvm-link -o "$lib/$c.bc" "$lib/$c.ll" \
            acats/report.ll >/dev/null 2>&1 || continue
        ( cd "$lib" && exec timeout "$TEST_TIMEOUT" lli "$c.bc" ) >/dev/null 2>&1 || true
    done
}

run_one(){
    local f=$1 n=$(basename "$1" .ada) q=${1##*/}; q=${q:0:1}
    # Fragments (end in digit, not 'm') are compiled by their family's main.
    [[ $n =~ [0-9]$ && ! $n =~ m$ ]] && return
    # Letter-suffixed fragments (d64005ea) belong to a <base><digit>m main.
    if [[ $n =~ ^(.*[a-z])[a-z]$ ]]; then
        compgen -G "acats/${BASH_REMATCH[1]}[0-9]m.ada" >/dev/null && return
    fi
    # Support packages (check_file, enum_check, spprt13, …) — never contain the
    # test-name form; ACATS test names have no underscore.
    [[ $n == *_* ]] && return
    local COMPILE_FILES MAIN_LL LINK_FRAGMENTS COMPILE_FAILED BIND_FAILED
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
        if ! link_program "$n"; then
            [[ $LINK_STATUS == timeout ]] && echo "c fail $n TIMEOUT:llvm-link_exceeded_${LINK_TIMEOUT}s" \
                                          || echo "c skip $n BIND:unresolved_symbols"
            return
        fi
        run_continuity_creators "$n" "$RESULTS_DIR/$n.lib"
        local rc=0
        run_in_lib "$TEST_TIMEOUT" "$n" > $LOGS_DIR/$n.out 2>&1 || rc=$?
        if ((rc==124 || rc==137)); then
            echo "c fail $n TIMEOUT:exceeded_${TEST_TIMEOUT}s"
            return
        fi
        # mcjit retry is for ORC-JIT crashes only; a timeout must not pay twice.
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
            elif grep -q '^NOT APPLICABLE:' $LOGS_DIR/$n.out 2>/dev/null; then
                # Only REPORT.NOT_APPLICABLE's own line counts; a COMMENT
                # merely mentioning the words must not mask a failure.
                echo "c skip $n N/A:$(grep -o '^NOT APPLICABLE:.*' $LOGS_DIR/$n.out|head -1|cut -c1-40)"
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
        if ! link_program "$n"; then
            [[ $LINK_STATUS == timeout ]] && echo "a fail $n TIMEOUT:llvm-link_exceeded_${LINK_TIMEOUT}s" \
                                          || echo "a skip $n BIND:unresolved_symbols"
            return; fi
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
        if ! link_program "$n"; then
            [[ $LINK_STATUS == timeout ]] && echo "d fail $n TIMEOUT:llvm-link_exceeded_${LINK_TIMEOUT}s" \
                                          || echo "d skip $n BIND"
            return; fi
        if run_in_lib "$TEST_TIMEOUT" "$n" > $LOGS_DIR/$n.out 2>&1 && grep -q PASSED $LOGS_DIR/$n.out; then
            echo "d pass $n PASSED"
        else
            echo "d fail $n FAILED:exact_arithmetic_check"
        fi
        ;;
    e)
        if ! compile_set "$n"; then
            echo "e skip $n COMPILE[$COMPILE_FAILED]:$(head -1 $LOGS_DIR/$n.err 2>/dev/null|cut -c1-50)"; return; fi
        if [[ -n $BIND_FAILED ]]; then
            echo "e skip $n BIND_REJECT:$(head -1 $LOGS_DIR/$n.bind 2>/dev/null|cut -c1-50)"; return; fi
        if ! link_program "$n"; then
            [[ $LINK_STATUS == timeout ]] && echo "e fail $n TIMEOUT:llvm-link_exceeded_${LINK_TIMEOUT}s" \
                                          || echo "e skip $n BIND"
            return; fi
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
            if link_program "$n"; then
                if run_in_lib 1 "$n" > $LOGS_DIR/$n.out 2>&1; then
                    echo "l fail $n WRONG_EXEC:should_not_execute"
                else
                    echo "l pass $n BIND_REJECT:execution_blocked"
                fi
            elif [[ $LINK_STATUS == timeout ]]; then
                echo "l fail $n TIMEOUT:llvm-link_exceeded_${LINK_TIMEOUT}s"
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
export -f run_one gather_files compile_set run_in_lib link_program run_continuity_creators pct
export START_MS TEST_TIMEOUT LINK_TIMEOUT

# Outer per-test cap (defense in depth): a hung test can never hang the suite,
# even if an inner `timeout` is bypassed by a runaway grandchild. On a
# SIGTERM/SIGKILL kill (124/137) emit a synthetic fail line so the test is
# recorded, never silently dropped.
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
}
export -f run_one_timed

# ═══════════════════════════════════════════════════════════════════════════
# REPORTING & BASELINE
# ═══════════════════════════════════════════════════════════════════════════

# Read a "class result name detail" stream into RESULTS_TSV (name<TAB>class<TAB>
# result<TAB>detail) and print the absolute per-class summary.
tally_results(){
    local results_file=$1
    local -A C=([a]=0 [b]=0 [c]=0 [d]=0 [e]=0 [l]=0
                [fa]=0 [fb]=0 [fc]=0 [fd]=0 [fe]=0 [fl]=0
                [sa]=0 [sb]=0 [sc]=0 [sd]=0 [se]=0 [sl]=0
                [ta]=0 [tb]=0 [tc]=0 [td]=0 [te]=0 [tl]=0
                [f]=0 [s]=0 [z]=0)
    : > "$RESULTS_TSV"

    local cls result name detail k
    while read -r cls result name detail; do
        [[ -z $cls ]] && continue
        k=${cls,,}
        printf '%s\t%s\t%s\t%s\n' "$name" "$k" "$result" "$detail" >> "$RESULTS_TSV"
        ((++C[z]))
        ((++C[t$k])) 2>/dev/null || C[t$k]=1
        case $result in
            pass) ((++C[$k])) ;;
            fail) ((++C[f])); ((++C[f$k])) 2>/dev/null || C[f$k]=1 ;;
            skip) ((++C[s])); ((++C[s$k])) 2>/dev/null || C[s$k]=1 ;;
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

    if [[ ${TAP:-0} == 1 ]]; then
        { echo "TAP version 13"; local i=0 name _k result detail
          while IFS=$'\t' read -r name _k result detail; do ((++i))
            case $result in
              pass) echo "ok $i $name" ;;
              skip) echo "ok $i $name # SKIP ${detail}" ;;
              *)    echo "not ok $i $name # ${detail}" ;;
            esac
          done < "$RESULTS_TSV"
          echo "1..$i"; } > "$RESULTS_DIR/results.tap"
    fi
}

# Diff RESULTS_TSV against the baseline manifest (name<TAB>status). Prints the
# categorized delta and sets REGRESSIONS to the count of pass→not-pass changes.
# Only "pass" counts as a good state, so the taxonomy is unambiguous:
#   REGRESSION   baseline pass, now not pass        (gates CI)
#   PROGRESSION  baseline not pass, now pass        (bless to lock in)
#   CHANGED      fail<->skip (neither was a pass)   (informational)
#   NEW          not in baseline                    (informational)
#   MISSING      in baseline, not run this time     (informational)
compare_to_baseline(){
    REGRESSIONS=0
    if [[ ! -f $BASELINE ]]; then
        printf "\nNo baseline at %s — run \`%s bless\` to create one.\n" "$BASELINE" "${0##*/}"
        return 0
    fi
    local -A BL RES
    local name status _c _d _rest
    while IFS=$'\t' read -r name status _rest; do [[ -n $name ]] && BL[$name]=$status; done < "$BASELINE"
    while IFS=$'\t' read -r name _c status _d; do [[ -n $name ]] && RES[$name]=$status; done < "$RESULTS_TSV"

    local -a regr=() prog=() chg=() new=() miss=()
    local k b r
    for k in "${!RES[@]}"; do
        b=${BL[$k]:-__absent__}; r=${RES[$k]}
        if   [[ $b == __absent__ ]]; then new+=("$k $r")
        elif [[ $b == "$r" ]];      then :
        elif [[ $b == pass ]];      then regr+=("$k $b->$r")
        elif [[ $r == pass ]];      then prog+=("$k $b->$r")
        else                             chg+=("$k $b->$r"); fi
    done
    for k in "${!BL[@]}"; do [[ -z ${RES[$k]:-} ]] && miss+=("$k ${BL[$k]}"); done
    REGRESSIONS=${#regr[@]}

    printf "\n========================================\nBASELINE DIFF  (%s)\n========================================\n" "$BASELINE"
    _emit(){ local tag=$1; shift; (($#)) || return 0
             printf "\n%s (%d):\n" "$tag" "$#"; printf '  %s\n' "$@" | sort; }
    _emit "REGRESSIONS"  ${regr[@]+"${regr[@]}"}
    _emit "PROGRESSIONS" ${prog[@]+"${prog[@]}"}
    _emit "CHANGED"      ${chg[@]+"${chg[@]}"}
    _emit "NEW"          ${new[@]+"${new[@]}"}
    _emit "MISSING"      ${miss[@]+"${miss[@]}"}
    printf "\n%d regression(s), %d progression(s), %d changed, %d new, %d missing.\n" \
        ${#regr[@]} ${#prog[@]} ${#chg[@]} ${#new[@]} ${#miss[@]}
    ((REGRESSIONS==0)) && printf "OK — no regressions vs baseline.\n" \
                       || printf "REGRESSED — %d test(s) that passed in the baseline now fail.\n" "$REGRESSIONS"
}

# Merge RESULTS_TSV into the baseline. A selector runs only part of the suite,
# so entries for tests NOT in this run are preserved; tests that ran are
# overwritten with their fresh status. Result: sorted `name<TAB>status`.
write_baseline(){
    local tmp; tmp=$(mktemp)
    [[ -f $BASELINE ]] && cp "$BASELINE" "$tmp"
    local -A NEW
    local name _c status _d _rest
    while IFS=$'\t' read -r name _c status _d; do [[ -n $name ]] && NEW[$name]=$status; done < "$RESULTS_TSV"
    { [[ -f $tmp ]] && while IFS=$'\t' read -r name status _rest; do
          [[ -n $name && -z ${NEW[$name]:-} ]] && printf '%s\t%s\n' "$name" "$status"
      done < "$tmp"
      for name in "${!NEW[@]}"; do printf '%s\t%s\n' "$name" "${NEW[$name]}"; done
    } | sort -k1,1 > "$BASELINE"
    rm -f "$tmp"
    printf "\nBaseline written: %s (%d tests recorded).\n" "$BASELINE" "$(wc -l < "$BASELINE")"
}

# ═══════════════════════════════════════════════════════════════════════════
# SELECTORS & DRIVER
# ═══════════════════════════════════════════════════════════════════════════

# Expand a selector to a shell glob of test files.
selector_glob(){
    case ${1:-all} in
        all)                     echo "acats/*.ada" ;;
        a|b|c|d|e|l|A|B|C|D|E|L)  echo "acats/${1,,}*.ada" ;;
        *)                       echo "acats/${1}*.ada" ;;
    esac
}

# Run the selector's tests in parallel, writing results.tsv + summary, then
# print the absolute report. Leaves the sorted result stream in RESULTS_TSV.
run_selector(){
    local sel=${1:-all} title=$2
    local pattern; pattern=$(selector_glob "$sel")
    local run_id="${sel}-$(date +%Y%m%d-%H%M%S)-$$"
    export RESULTS_DIR="test_results/${run_id}"
    export LOGS_DIR="acats_logs/${run_id}"
    export RESULTS_TSV="$RESULTS_DIR/results.tsv"
    mkdir -p "$RESULTS_DIR" "$LOGS_DIR"

    printf "\n========================================\n%s\n========================================\n\n" "$title"
    printf "results: %s\nlogs:    %s\n\n" "$RESULTS_DIR" "$LOGS_DIR"

    local tmpfile; tmpfile=$(mktemp)
    for f in $pattern; do [[ -f $f ]] && echo "$f"; done \
        | xargs -P "$NPROC" -I{} bash -c 'run_one_timed "$@"' _ {} > "$tmpfile" 2>/dev/null
    sort -k3 "$tmpfile" > "${tmpfile}.sorted"
    tally_results "${tmpfile}.sorted"
    rm -f "$tmpfile" "${tmpfile}.sorted"
}

usage(){ sed -n '3,44p' "$0" | sed 's/^# \{0,1\}//'; }

main(){
    local cmd=${1:-help}; shift || true
    case $cmd in
        run|g)   run_selector "${1:-all}" "ACATS run — ${1:-all}" ;;
        q)       run_selector "${1:-c32}" "ACATS run — ${1:-c32}" ;;
        check)   run_selector "${1:-all}" "ACATS check — ${1:-all}"
                 compare_to_baseline
                 ((REGRESSIONS==0)) || exit 1 ;;
        bless)   run_selector "${1:-all}" "ACATS bless — ${1:-all}"
                 write_baseline ;;
        list)    local pattern; pattern=$(selector_glob "${1:-all}")
                 for f in $pattern; do [[ -f $f ]] && basename "$f" .ada; done ;;
        h|help|-h|--help) usage ;;
        *)       usage; exit 2 ;;
    esac
}
main "$@"
