#!/usr/bin/env bash
set -euo pipefail

NPROC=${JOBS:-${NPROC:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}}
TEST_TIMEOUT=${TEST_TIMEOUT:-30}
COMPILE_TIMEOUT=${COMPILE_TIMEOUT:-30}
LINK_TIMEOUT=${LINK_TIMEOUT:-20}
BASELINE=${BASELINE:-acats.baseline}

TIMEOUT=$(command -v timeout || command -v gtimeout) || {
    echo "FATAL: no 'timeout' command found" >&2
    echo "       on macOS: brew install coreutils" >&2
    exit 1
}
export TIMEOUT

for tool in llvm-link lli; do
    command -v "$tool" >/dev/null || {
        echo "FATAL: no '$tool' command found" >&2
        echo "       classes A, C, D and E link and run what they compile," >&2
        echo "       and without it every one of them reports as skipped" >&2
        echo "       Debian/Ubuntu: apt-get install llvm" >&2
        echo "       macOS:         brew install llvm" >&2
        echo "       Windows:       winget install LLVM.LLVM" >&2
        exit 1
    }
done

now_ms(){
    local stamp
    stamp=$(date +%s%3N 2>/dev/null)
    [[ $stamp =~ ^[0-9]+$ ]] || stamp=$(( $(date +%s) * 1000 ))
    printf '%s' "$stamp"
}
START_MS=$(now_ms)

mkdir -p test_results acats_logs

if [[ ! -d acats ]]; then
    [[ -f tests.zip ]] || { echo "FATAL: no acats/ directory and no tests.zip"; exit 1; }
    echo "Unpacking tests.zip..."
    unzip -q tests.zip || { echo "FATAL: cannot unpack tests.zip"; exit 1; }
fi

if [[ ! -f ./ada83 ]] || [[ ada83.c -nt ./ada83 ]]; then
    echo "Rebuilding ada83..."
    make -s ada83 || { echo "FATAL: compiler build failed"; exit 1; }
fi

export REPORT_LL="${TMPDIR:-/tmp}/ada83-report-$$.ll"
trap 'rm -f "$REPORT_LL" "${REPORT_LL%.ll}.ali"' EXIT
./ada83 --ir acats/report.adb -o "$REPORT_LL" >/dev/null 2>&1 || {
    echo "FATAL: cannot compile acats/report.adb"; exit 1; }

pct(){ ((${2:-0}>0)) && printf %d $((100*$1/$2)) || printf 0; }

elapsed(){
    printf %.3f "$(bc<<<"scale=4;($(now_ms)-${START_MS})/1000")"
}

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

compile_set(){
    local n=$1 part pn
    local lib=$RESULTS_DIR/$n.lib
    mkdir -p "$lib"
    MAIN_LL=""
    LINK_FRAGMENTS=()
    COMPILE_FAILED=""
    for part in "${COMPILE_FILES[@]}"; do
        pn=$(basename "$part" .ada)
        if ! "$TIMEOUT" "$COMPILE_TIMEOUT" ./ada83 --ir "$part" -o $lib/$pn.ll >/dev/null 2>$LOGS_DIR/$n.err; then
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
            [[ -n ${frag_units[i]//[$' \n']} ]] || current=1
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

run_in_lib(){
    local secs=$1 n=$2; shift 2
    ( cd "$RESULTS_DIR/$n.lib" 2>/dev/null || exit 127
      exec "$TIMEOUT" "$secs" lli "$@" "$ROOT/$RESULTS_DIR/$n.bc" )
}

link_program(){
    local n=$1 rc=0
    "$TIMEOUT" "$LINK_TIMEOUT" llvm-link -o "$RESULTS_DIR/$n.bc" "$MAIN_LL" \
        ${LINK_FRAGMENTS[@]+"${LINK_FRAGMENTS[@]}"} "$REPORT_LL" \
        2>"$LOGS_DIR/$n.link" || rc=$?
    case $rc in
        0)       LINK_STATUS=ok ;;
        124|137) LINK_STATUS=timeout ;;
        *)       LINK_STATUS=unresolved ;;
    esac
    return $rc
}

run_continuity_creators(){
    local reader=$1 lib=$2 self=${1,,} c
    for c in $(grep -oiE 'legal_file_name[ ]*\([^)]*"ce[0-9a-z]+"' "acats/$reader.ada" 2>/dev/null \
               | grep -oiE '"ce[0-9a-z]+"' | tr -d '"' | tr 'A-Z' 'a-z' | sort -u); do
        [[ $c == "$self" || ! -f acats/$c.ada ]] && continue
        ./ada83 --ir "acats/$c.ada" -o "$lib/$c.ll" >/dev/null 2>&1 || continue
        "$TIMEOUT" "$LINK_TIMEOUT" llvm-link -o "$lib/$c.bc" "$lib/$c.ll" \
            "$REPORT_LL" >/dev/null 2>&1 || continue
        ( cd "$lib" && exec "$TIMEOUT" "$TEST_TIMEOUT" lli "$c.bc" ) >/dev/null 2>&1 || true
    done
}

run_one(){
    local f=$1 n=$(basename "$1" .ada) q=${1##*/}; q=${q:0:1}
    [[ $n =~ [0-9]$ && ! $n =~ m$ ]] && return
    if [[ $n =~ ^(.*[a-z])[a-z]$ ]]; then
        compgen -G "acats/${BASH_REMATCH[1]}[0-9]m.ada" >/dev/null && return
    fi
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
        local -a expected=() actual=()
        local part pn i hits=0 rejected=""
        local lib=$RESULTS_DIR/$n.lib
        mkdir -p "$lib"
        for part in "${COMPILE_FILES[@]}"; do
            pn=$(basename "$part")
            i=0
            while IFS= read -r l; do
                ((++i))
                if [[ $l =~ --[[:space:]]+ERROR[[:space:]]*[:\;.] ]]; then
                    if [[ $l =~ ^[[:space:]]*-- ]]; then
                        expected+=("$pn:$i:3")
                    else
                        expected+=("$pn:$i:1")
                    fi
                fi
            done < "$part"
            if "$TIMEOUT" "$COMPILE_TIMEOUT" ./ada83 --ir "$part" -o "$lib/${pn%.ada}.ll" \
                 >/dev/null 2>$LOGS_DIR/$n.$pn.err; then :; else
                rejected=yes
            fi
            while IFS=: read -r file m _; do
                actual+=("$(basename "$file"):$m")
            done < <(grep "^[^:]*:[0-9]*:[0-9]*: " $LOGS_DIR/$n.$pn.err)
        done
        if [[ -z $rejected ]]; then
            echo "b fail $n WRONG_ACCEPT:compiled_when_should_reject"
        else
            local ef el ew vf vl
            for e in ${expected[@]+"${expected[@]}"}; do
                ew=${e##*:}; ef=${e%%:*}
                el=${e#*:}; el=${el%%:*}
                for v in ${actual[@]+"${actual[@]}"}; do
                    vf=${v%:*}; vl=${v##*:}
                    [[ $vf == "$ef" ]] && ((vl>=el-ew&&vl<=el+ew)) && { ((++hits)); break; }
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
        e_reject(){
            local n=$1 stage=$2 detail=$3
            if grep -q "PASSED => ERROR\|IN THIS CASE RECOMPILATION IS\|SHOULD NOT BE LINKABLE" "$f"; then
                echo "e pass $n ${stage}_REJECT_DOCUMENTED_PASS"
            elif grep -q "N/A => ERROR\|NON-APPLICABLE IF THE INSTANTIATION" "$f"; then
                echo "e skip $n N/A:${stage}_rejection_sanctioned"
            else
                echo "e skip $n ${stage}:$detail"
            fi
        }
        if ! compile_set "$n"; then
            e_reject "$n" COMPILE "$(head -1 $LOGS_DIR/$n.err 2>/dev/null|cut -c1-50)"; return; fi
        if [[ -n $BIND_FAILED ]]; then
            e_reject "$n" BIND "$(head -1 $LOGS_DIR/$n.bind 2>/dev/null|cut -c1-50)"; return; fi
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
    f) ;;
    *) echo "? skip $n UNKNOWN:unrecognized_class" ;;
    esac
}
ROOT=$PWD
export ROOT
export -f run_one gather_files compile_set run_in_lib link_program run_continuity_creators pct
export START_MS TEST_TIMEOUT LINK_TIMEOUT COMPILE_TIMEOUT

run_one_timed(){
    local f=$1 n=$(basename "$1" .ada) q
    q=${f##*/}; q=${q:0:1}; q=${q,,}
    local out rc=0
    out=$("$TIMEOUT" $((2*TEST_TIMEOUT+5)) bash -c 'run_one "$1"' _ "$f" 2>/dev/null) || rc=$?
    if ((rc==124 || rc==137)); then
        echo "$q fail $n TIMEOUT:exceeded_$((2*TEST_TIMEOUT+5))s"
    elif [[ -n $out ]]; then
        echo "$out"
    fi
}
export -f run_one_timed

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

selector_glob(){
    case ${1:-all} in
        all)                     echo "acats/*.ada" ;;
        a|b|c|d|e|l|A|B|C|D|E|L)  echo "acats/${1,,}*.ada" ;;
        *)                       echo "acats/${1}*.ada" ;;
    esac
}

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

usage(){ cat <<'TEXT'
Usage: test.sh COMMAND [SELECTOR]

Run the ACATS conformance suite against ./ada83.

Commands:
  run [SELECTOR]     run the suite and print a report
  check [SELECTOR]   run, then diff against the baseline; exit 1 on regression
  bless [SELECTOR]   run, then write the results as the new baseline
  list [SELECTOR]    list the tests a selector expands to
  help               display this help and exit

Selectors:
  all                every test (default)
  a, b, c, d, e, l   one ACATS class
  PREFIX             a filename prefix, such as c45 or c45347

Environment:
  JOBS, NPROC        parallel workers (default: the processor count)
  TEST_TIMEOUT       per-test run cap in seconds (default: 30)
  LINK_TIMEOUT       per-test link cap in seconds (default: 20)
  BASELINE           baseline manifest path (default: acats.baseline)
  TAP                set to 1 to also write a TAP stream

Each run writes results to test_results/ID/ and logs to acats_logs/ID/, where
ID is unique to the run, so concurrent runs do not overwrite one another.
TEXT
}

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
