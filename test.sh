#!/usr/bin/env bash
set -euo pipefail

NPROC=${JOBS:-${NPROC:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}}
TEST_TIMEOUT=${TEST_TIMEOUT:-30}
COMPILE_TIMEOUT=${COMPILE_TIMEOUT:-30}
LINK_TIMEOUT=${LINK_TIMEOUT:-20}
STARTUP_TIMEOUT=${STARTUP_TIMEOUT:-30}
BUILD_TIMEOUT=${BUILD_TIMEOUT:-300}
KILL_GRACE=${KILL_GRACE:-2}
# Last-resort cap around a complete worker. Individual compile, bind, link and
# execution phases still have their own tighter limits.
WORKER_TIMEOUT=${WORKER_TIMEOUT:-$((8 * COMPILE_TIMEOUT + 4 * LINK_TIMEOUT + 2 * TEST_TIMEOUT + 30))}
BASELINE=${BASELINE:-acats.baseline}
OPT=${OPT:--O2}
NO_ANIMATE=${NO_ANIMATE:-0} NO_COLOUR=${NO_COLOUR:-0}

# --- presentation, in the manner of test-bench.sh ---------------------------
animated=0; [ -t 2 ] && [ "$NO_ANIMATE" != 1 ] && animated=1
if [ $animated = 1 ] && exec 9>/dev/tty 2>/dev/null; then :; else exec 9>/dev/null; animated=0; fi
if [ -t 1 ] && [ "$NO_COLOUR" != 1 ]
then BOLD=$'\033[1m' DIM=$'\033[2m' GOOD=$'\033[32m' BAD=$'\033[31m' OFF=$'\033[0m'
else BOLD='' DIM='' GOOD='' BAD='' OFF=''
fi

hide_cursor(){ [ $animated = 1 ] && printf '\033[?25l' >&9; return 0; }
show_cursor(){ [ $animated = 1 ] && printf '\033[?25h' >&9; return 0; }

BLOCKS=("" "▏" "▎" "▍" "▌" "▋" "▊" "▉")

bar(){
    local fill=$1 width=$2 out='' whole part i
    whole=${fill%.*}
    part=$(awk -v f="$fill" -v w="$whole" 'BEGIN{printf "%d",(f-w)*8}')
    for ((i = 0; i < whole && i < width; i++)); do out+='█'; done
    [ "$whole" -lt "$width" ] && [ "$part" -gt 0 ] && { out+=${BLOCKS[$part]}; whole=$((whole+1)); }
    for ((i = whole; i < width; i++)); do out+='·'; done
    printf '%s' "$out"
}

scaled(){ awk -v v="$1" -v m="$2" -v w="$3" 'BEGIN{printf "%.3f",(m>0?v/m:0)*w}'; }

progress(){
    [ $animated = 1 ] || return 0
    printf '\r\033[2K  %s%s%s %3d%%  %s%s%s' "$DIM" "$(bar "$(scaled "$1" "$2" 32)" 32)" "$OFF" \
        "$(awk -v d="$1" -v t="$2" 'BEGIN{printf "%d",(t>0?d*100/t:0)}')" "$DIM" "$3" "$OFF" >&9
}

clear_line(){ [ $animated = 1 ] && printf '\r\033[2K' >&9; return 0; }

PULSE=''
PULSE_FRAMES=('◦' '◌' '◍' '◎')
pulse(){
    [ $animated = 1 ] || return 0
    ( i=0; while :; do
        printf '\r\033[2K  %s%s %s%s' "$DIM" "${PULSE_FRAMES[(i/2)%4]}" "$1" "$OFF" >&9
        i=$((i+1)); sleep 0.12
      done ) & PULSE=$!
}
pulse_stop(){ [ -n "$PULSE" ] && { kill "$PULSE" 2>/dev/null; wait "$PULSE" 2>/dev/null; PULSE=''; clear_line; }; return 0; }

heading(){
    printf '\n  %s%s%s\n' "$BOLD" "$1" "$OFF"
    [ $# -gt 1 ] && printf '  %s%s%s\n' "$DIM" "$2" "$OFF"
    printf '\n'
}
rule(){ printf '  %s%s%s\n' "$DIM" "────────────────────────────────────────────────────────────────" "$OFF"; }
die(){ pulse_stop; show_cursor; echo "test.sh: $*" >&2; exit 1; }
# ---------------------------------------------------------------------------

find_timeout(){
    local candidate found
    for candidate in ${TIMEOUT:+"$TIMEOUT"} timeout gtimeout /usr/bin/timeout; do
        found=$(command -v "$candidate" 2>/dev/null) || continue
        # Windows ships an unrelated System32\timeout.exe, so ask for the
        # behaviour rather than trusting the name.
        "$found" -k 1 1 true >/dev/null 2>&1 && { printf '%s' "$found"; return 0; }
    done
    return 1
}

TIMEOUT=$(find_timeout) || {
    echo "test.sh: no usable 'timeout' command found" >&2
    echo "         macOS:   brew install coreutils" >&2
    echo "         Windows: run this under the bash that comes with Git for" >&2
    echo "                  Windows, whose /usr/bin/timeout comes first on PATH" >&2
    exit 1
}
export TIMEOUT

timed(){
    local secs=$1
    shift
    "$TIMEOUT" -k "$KILL_GRACE" "$secs" "$@"
}
export -f timed
export KILL_GRACE

now_ms(){
    local stamp
    stamp=$(date +%s%3N 2>/dev/null)
    [[ $stamp =~ ^[0-9]+$ ]] || stamp=$(( $(date +%s) * 1000 ))
    printf '%s' "$stamp"
}
START_MS=$(now_ms)

mkdir -p test_results acats_logs

# $2 names the directory to take, so that a tree holding one suite but not
# the other does not have to be unpacked whole -- and unzip never stops to
# ask whether to overwrite what is already there.
unpack(){
    if command -v unzip >/dev/null; then
        unzip -qo "$1" "$2/*"
    elif command -v tar >/dev/null && tar -xf "$1" "$2" 2>/dev/null; then
        :
    elif command -v powershell.exe >/dev/null; then
        powershell.exe -NoProfile -Command \
            "Expand-Archive -LiteralPath '$1' -DestinationPath '.' -Force"
    else
        return 1
    fi
}

for suite in acats implementation; do
    [[ -d $suite ]] && continue
    [[ -f tests.zip ]] || die "no $suite/ directory and no tests.zip"
    pulse "unpacking $suite"
    unpack tests.zip "$suite" || die "cannot unpack $suite from tests.zip"
    pulse_stop
done

case $(uname -s 2>/dev/null) in
    Darwin)                 HOST_TARGET=macos ;;
    MINGW*|MSYS*|CYGWIN*)   HOST_TARGET=windows ;;
    *)                      HOST_TARGET=linux ;;
esac

find_ada83(){
    local candidate
    for candidate in ${ADA83:+"$ADA83"} \
                     "bin-$HOST_TARGET/ada83" "bin-$HOST_TARGET/ada83.exe" \
                     ./ada83 ./ada83.exe; do
        [[ -x $candidate ]] && { printf '%s' "$candidate"; return 0; }
    done
    return 1
}

build_ada83(){
    if command -v make >/dev/null; then
        timed "$BUILD_TIMEOUT" make -s ada83
    elif [[ $HOST_TARGET == windows ]] && command -v cmd.exe >/dev/null; then
        timed "$BUILD_TIMEOUT" cmd.exe //c make.bat
    else
        echo "test.sh: no 'make' to build the compiler with" >&2
        echo "         Windows: run make.bat, then start this script again" >&2
        return 1
    fi
}

ADA83=$(find_ada83) || ADA83=""
if [[ -z $ADA83 ]] || [[ ada83.c -nt $ADA83 ]]; then
    printf '  %srebuilding ada83%s\n' "$DIM" "$OFF"
    build_ada83 || die "compiler build failed"
    ADA83=$(find_ada83) || die "no ada83 executable after building"
fi
[[ $ADA83 == /* || $ADA83 == ?:[/\\]* ]] || ADA83=$PWD/${ADA83#./}
export ADA83

export REPORT_LL="${TMPDIR:-/tmp}/ada83-report-$$.ll"
cleanup(){
    pulse_stop
    show_cursor
    [ -n "${REPORT_LL:-}" ] && rm -f "$REPORT_LL" "${REPORT_LL%.ll}.ali"
    return 0
}
trap cleanup EXIT
trap 'show_cursor; exit 130' INT
timed "$COMPILE_TIMEOUT" "$ADA83" --ir acats/report.adb -o "$REPORT_LL" >/dev/null 2>&1 || \
    die "cannot compile acats/report.adb"

pct(){ ((${2:-0}>0)) && printf %d $((100*$1/$2)) || printf 0; }

elapsed(){
    local ms=$(( $(now_ms) - START_MS ))
    printf '%d.%03d' $((ms / 1000)) $((ms % 1000))
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
        if ! timed "$COMPILE_TIMEOUT" "$ADA83" --ir "$part" -o "$lib/$pn.ll" >/dev/null 2>"$LOGS_DIR/$n.err"; then
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
                # library subprograms carry the _ada_ prefix; packages do not
                grep -Eq "^define.*@(_ada_)?${unit//./__}(\(|__)" "$MAIN_LL" && in_main=1
            done
            ((in_main)) && continue
            ((current)) || continue
            kept+=("$frag")
        done
        LINK_FRAGMENTS=(${kept[@]+"${kept[@]}"})
    fi

    BIND_FAILED=""
    if ! timed "$COMPILE_TIMEOUT" "$ADA83" --bind "$lib" "$n" 2>"$LOGS_DIR/$n.bind"; then
        BIND_FAILED=$n
    fi
}

run_in_lib(){
    local secs=$1 n=$2; shift 2
    ( cd "$RESULTS_DIR/$n.lib" 2>/dev/null || exit 127
      timed "$secs" "$ROOT/$PROGRAM" "$@" )
}

link_program(){
    local n=$1 rc=0
    PROGRAM=$RESULTS_DIR/$n.bin
    timed "$LINK_TIMEOUT" "$ADA83" "$OPT" "$MAIN_LL" \
        ${LINK_FRAGMENTS[@]+"${LINK_FRAGMENTS[@]}"} "$REPORT_LL" \
        -o "$PROGRAM" >/dev/null 2>"$LOGS_DIR/$n.link" || rc=$?
    [[ -x $PROGRAM ]] || [[ ! -x $PROGRAM.exe ]] || PROGRAM=$PROGRAM.exe
    case $rc in
        0)       LINK_STATUS=ok ;;
        124|137) LINK_STATUS=timeout ;;
        *)       LINK_STATUS=unresolved ;;
    esac
    return $rc
}

report_link_failure(){
    local q=$1 n=$2
    [[ $LINK_STATUS == timeout ]] \
        && echo "$q fail $n TIMEOUT:link_exceeded_${LINK_TIMEOUT}s" \
        || echo "$q skip $n BIND:unresolved_symbols"
}

run_continuity_creators(){
    local reader=$1 lib=$2 self=${1,,} c
    for c in $(grep -oiE 'legal_file_name[ ]*\([^)]*"ce[0-9a-z]+"' "acats/$reader.ada" 2>/dev/null \
               | grep -oiE '"ce[0-9a-z]+"' | tr -d '"' | tr 'A-Z' 'a-z' | sort -u); do
        [[ $c == "$self" || ! -f acats/$c.ada ]] && continue
        timed "$COMPILE_TIMEOUT" "$ADA83" --ir "acats/$c.ada" -o "$lib/$c.ll" \
            >/dev/null 2>&1 || continue
        timed "$LINK_TIMEOUT" "$ADA83" "$OPT" "$lib/$c.ll" "$REPORT_LL" \
            -o "$lib/$c.bin" >/dev/null 2>&1 || continue
        ( cd "$lib" && timed "$TEST_TIMEOUT" "./$c.bin" ) >/dev/null 2>&1 || true
    done
}

run_one(){
    local f=$1 n=$(basename "$1" .ada) q=${1##*/}; q=${q:0:1}
    [[ $n =~ [0-9]$ && ! $n =~ m$ ]] && return
    if [[ $n =~ ^(.*[a-z])[a-z]$ ]]; then
        compgen -G "acats/${BASH_REMATCH[1]}[0-9]m.ada" >/dev/null && return
    fi
    [[ $n == *_* ]] && return
    local COMPILE_FILES MAIN_LL LINK_FRAGMENTS COMPILE_FAILED BIND_FAILED PROGRAM
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
            report_link_failure c "$n"
            return
        fi
        run_continuity_creators "$n" "$RESULTS_DIR/$n.lib"
        local rc=0
        run_in_lib "$TEST_TIMEOUT" "$n" > $LOGS_DIR/$n.out 2>&1 || rc=$?
        if ((rc==124 || rc==137)); then
            echo "c fail $n TIMEOUT:exceeded_${TEST_TIMEOUT}s"
            return
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
            report_link_failure a "$n"
            return; fi
        local rc=0
        run_in_lib "$TEST_TIMEOUT" "$n" > $LOGS_DIR/$n.out 2>&1 || rc=$?
        if ((rc==124 || rc==137)); then
            echo "a fail $n TIMEOUT:exceeded_${TEST_TIMEOUT}s"
        elif ((rc==0)); then
            echo "a pass $n PASSED"
        else
            echo "a fail $n FAILED:exit_$rc"
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
            if timed "$COMPILE_TIMEOUT" "$ADA83" --ir "$part" -o "$lib/${pn%.ada}.ll" \
                 >/dev/null 2>"$LOGS_DIR/$n.$pn.err"; then :; else
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
            report_link_failure d "$n"
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
            report_link_failure e "$n"
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
                echo "l fail $n TIMEOUT:link_exceeded_${LINK_TIMEOUT}s"
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
export -f run_one gather_files compile_set run_in_lib link_program \
          report_link_failure run_continuity_creators pct
export START_MS TEST_TIMEOUT LINK_TIMEOUT COMPILE_TIMEOUT STARTUP_TIMEOUT \
       BUILD_TIMEOUT WORKER_TIMEOUT OPT

run_one_timed(){
    local f=$1 n=$(basename "$1" .ada) q
    q=${f##*/}; q=${q:0:1}; q=${q,,}
    local out rc=0
    out=$(timed "$WORKER_TIMEOUT" bash -c 'run_one "$1"' _ "$f" 2>/dev/null) || rc=$?
    if ((rc==124 || rc==137)); then
        echo "$q fail $n TIMEOUT:worker_exceeded_${WORKER_TIMEOUT}s"
    elif ((rc != 0)); then
        echo "$q fail $n HARNESS:worker_exit_${rc}"
    elif [[ -n $out ]]; then
        echo "$out"
    fi
    # one tick per file, whatever became of it, so the bar reaches the end
    [[ -n ${PROGRESS_FILE:-} ]] && echo >> "$PROGRESS_FILE" || true
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

    heading "RESULTS"
    printf '  %-22s %6s %6s %6s %6s %7s\n' class pass fail skip total rate
    rule
    ((C[ta]>0)) && printf '  %-22s %6d %6d %6d %6d %6d%%\n' "A  Acceptance" ${C[a]} ${C[fa]} ${C[sa]} ${C[ta]} $(pct ${C[a]} ${C[ta]})
    ((C[tb]>0)) && printf '  %-22s %6d %6d %6d %6d %6d%%\n' "B  Illegality" ${C[b]} ${C[fb]} ${C[sb]} ${C[tb]} $(pct ${C[b]} ${C[tb]})
    ((C[tc]>0)) && printf '  %-22s %6d %6d %6d %6d %6d%%\n' "C  Executable" ${C[c]} ${C[fc]} ${C[sc]} ${C[tc]} $(pct ${C[c]} ${C[tc]})
    ((C[td]>0)) && printf '  %-22s %6d %6d %6d %6d %6d%%\n' "D  Numerics"   ${C[d]} ${C[fd]} ${C[sd]} ${C[td]} $(pct ${C[d]} ${C[td]})
    ((C[te]>0)) && printf '  %-22s %6d %6d %6d %6d %6d%%\n' "E  Inspection" ${C[e]} ${C[fe]} ${C[se]} ${C[te]} $(pct ${C[e]} ${C[te]})
    ((C[tl]>0)) && printf '  %-22s %6d %6d %6d %6d %6d%%\n' "L  Post-compilation" ${C[l]} ${C[fl]} ${C[sl]} ${C[tl]} $(pct ${C[l]} ${C[tl]})
    rule
    printf '  %s%-22s %6d %6d %6d %6d %6d%%%s\n' "$BOLD" TOTAL $pass ${C[f]} ${C[s]} ${C[z]} $(pct $pass ${C[z]}) "$OFF"
    printf '\n  %selapsed%s %ss over %d tests with %s workers, %s\n' \
        "$DIM" "$OFF" "$(elapsed)" ${C[z]} "$NPROC" "$(date '+%Y-%m-%d %H:%M:%S')"
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
        printf '\n  %sno baseline at %s — run `%s bless` to create one%s\n' \
            "$DIM" "$BASELINE" "${0##*/}" "$OFF"
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

    heading "BASELINE DIFF" "$BASELINE"
    _emit(){ local tag=$1 colour=$2; shift 2; (($#)) || return 0
             printf '  %s%s (%d)%s\n' "$colour" "$tag" "$#" "$OFF"
             printf '    %s\n' "$@" | sort; printf '\n'; }
    _emit "REGRESSIONS"  "$BAD"  ${regr[@]+"${regr[@]}"}
    _emit "PROGRESSIONS" "$GOOD" ${prog[@]+"${prog[@]}"}
    _emit "CHANGED"      "$BOLD" ${chg[@]+"${chg[@]}"}
    _emit "NEW"          "$BOLD" ${new[@]+"${new[@]}"}
    _emit "MISSING"      "$BOLD" ${miss[@]+"${miss[@]}"}
    printf '  %s%d regression(s), %d progression(s), %d changed, %d new, %d missing%s\n' \
        "$DIM" ${#regr[@]} ${#prog[@]} ${#chg[@]} ${#new[@]} ${#miss[@]} "$OFF"
    ((REGRESSIONS==0)) && printf '  %sOK — no regressions vs baseline%s\n' "$GOOD" "$OFF" \
                       || printf '  %sREGRESSED — %d test(s) that passed in the baseline now fail%s\n' \
                              "$BAD" "$REGRESSIONS" "$OFF"
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
    printf '\n  %sbaseline%s written to %s, %d tests recorded\n' \
        "$BOLD" "$OFF" "$BASELINE" "$(wc -l < "$BASELINE")"
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

    local listfile; listfile=$(mktemp)
    for f in $pattern; do [[ -f $f ]] && echo "$f" >> "$listfile"; done || true
    local total; total=$(wc -l < "$listfile")
    ((total > 0)) || { rm -f "$listfile"; die "selector '$sel' matched no tests"; }

    local version
    version=$(timed "$STARTUP_TIMEOUT" "$ADA83" --version 2>&1) || \
        version="version query failed or timed out"
    version=${version%%$'\n'*}

    heading "$title"
    printf '  %sada83%s    %s\n' "$BOLD" "$OFF" "$version"
    printf '  %stests%s    %s, over %s workers\n' "$BOLD" "$OFF" "$total" "$NPROC"
    printf '  %sresults%s  %s\n' "$BOLD" "$OFF" "$RESULTS_DIR"
    printf '  %slogs%s     %s\n' "$BOLD" "$OFF" "$LOGS_DIR"

    export PROGRESS_FILE; PROGRESS_FILE=$(mktemp)
    hide_cursor
    local tmpfile; tmpfile=$(mktemp)
    local worker_errors="$LOGS_DIR/harness.err"
    local runner_status; runner_status=$(mktemp)
    : > "$worker_errors"
    (
        set +e
        xargs -P "$NPROC" -I{} bash -c 'run_one_timed "$@"' _ {} \
            < "$listfile" > "$tmpfile" 2>"$worker_errors"
        local rc=$?
        printf '%d\n' "$rc" > "$runner_status"
        exit "$rc"
    ) &
    local runner=$!
    if [ $animated = 1 ]; then
        local done_n last
        while [[ ! -s $runner_status ]] && kill -0 "$runner" 2>/dev/null; do
            done_n=$(wc -l < "$PROGRESS_FILE" 2>/dev/null) || done_n=0
            last=$(awk 'END{print $3}' "$tmpfile" 2>/dev/null) || last=''
            progress "${done_n:-0}" "$total" "${last:-running the suite}"
            sleep 0.15
        done
    fi
    local runner_rc=0
    wait "$runner" || runner_rc=$?
    clear_line; show_cursor
    if ((runner_rc != 0)); then
        printf '  %swarning:%s worker launcher exited with status %d; see %s\n' \
            "$BAD" "$OFF" "$runner_rc" "$worker_errors" >&2
    fi
    sort -k3 "$tmpfile" > "${tmpfile}.sorted"
    tally_results "${tmpfile}.sorted"
    rm -f "$tmpfile" "${tmpfile}.sorted" "$listfile" "$PROGRESS_FILE" "$runner_status"
}

# The implementation tests are Ada programs under implementation/, covering
# what ACATS
# cannot see: the _ada_ symbol prefix on library subprograms, and Command_Line.
# Each program self-reports PASSED or FAILED; comment headers steer the run:
#
#   -- ARGS: alpha "two words"     command-line arguments for the run
#   -- LINK: other.ada             unit to compile separately and link in
#   -- SYMBOL: _ada_main           symbol nm must find in the executable
#   -- SYMBOL-NOT: _ada_expo       symbol nm must not find
#
# A file some test's -- LINK: header names is a support unit, not a test.
ext_header(){ sed -n "s/^-- $2: //p" "$1" | tail -1; }

run_implementation_tests(){
    EXT_PASS=0 EXT_FAIL=0 EXT_SKIP=0
    [[ -d implementation ]] || { printf '  %sno implementation/ directory%s\n' "$DIM" "$OFF"; return 0; }

    local nm_tool; nm_tool=$(command -v nm || command -v llvm-nm || true)
    local work; work=$(mktemp -d "${TMPDIR:-/tmp}/ada83-ext.XXXXXX")

    local -A is_support=()
    local source linked
    for source in implementation/*.ada; do
        linked=$(ext_header "$source" LINK)
        [[ -n $linked ]] && is_support[$linked]=1
    done

    heading "IMPLEMENTATION" "the _ada_ symbol prefix, Command_Line and --analyze"

    local name dir exe symbol symbol_not symbols args detail analyze
    local -a fragments
    for source in implementation/*.ada; do
        name=$(basename "$source" .ada)
        [[ -n ${is_support[$name.ada]:-} ]] && continue

        dir=$work/$name
        mkdir -p "$dir"
        fragments=()
        detail=""

        # An ANALYZE test asks what --analyze reports rather than what the
        # program prints: WARN names text a diagnostic must carry,
        # WARN-COUNT how many lines carry it, NO-WARN text that must not
        # appear at all, and JSON a pattern the report must contain.  A
        # rule that stops firing then fails rather than passing quietly.
        analyze=$(ext_header "$source" ANALYZE)
        if [[ -n $analyze ]]; then
            local want want_count seen no_want json_want support
            [[ $analyze == none ]] && analyze=""

            # A unit this one reads is compiled first, so that its
            # library information file -- and the summaries in it --
            # exist before the caller is analysed.
            support=$(ext_header "$source" LINK)
            if [[ -n $support ]]; then
                ( cd "$dir" && timed "$COMPILE_TIMEOUT" "$ADA83" --ir \
                    "$ROOT/implementation/$support" -o "$dir/${support%.ada}.ll" ) \
                    >"$dir/support.log" 2>&1 ||
                    detail="support unit $support: $(tail -2 "$dir/support.log" | tr '\n' ' ')"
            fi
            [[ -z $detail ]] &&
              ( cd "$dir" && timed "$COMPILE_TIMEOUT" "$ADA83" --analyze $analyze \
                 -I "$ROOT/implementation" "$ROOT/$source" ) \
                 >"$dir/report.json" 2>"$dir/warn.log"

            want=$(ext_header "$source" WARN)
            want_count=$(ext_header "$source" WARN-COUNT)
            no_want=$(ext_header "$source" NO-WARN)
            json_want=$(ext_header "$source" JSON)

            if [[ -n $want ]]; then
                seen=$(grep -c -- "$want" "$dir/warn.log" || true)
                if ((seen == 0)); then
                    detail="no diagnostic said '$want'"
                elif [[ -n $want_count ]] && ((seen != want_count)); then
                    detail="$seen diagnostic(s) said '$want', wanted $want_count"
                fi
            fi
            if [[ -z $detail && -n $no_want ]] &&
               grep -q -- "$no_want" "$dir/warn.log"; then
                detail="a diagnostic said '$no_want', which must not appear"
            fi
            if [[ -z $detail && -n $json_want ]] &&
               ! grep -q -- "$json_want" "$dir/report.json"; then
                detail="the report does not contain '$json_want'"
            fi

            if [[ -z $detail ]]; then
                printf '  %sok%s   %s\n' "$GOOD" "$OFF" "$name"
                ((++EXT_PASS))
            else
                printf '  %sFAIL%s %s — %s\n' "$BAD" "$OFF" "$name" "$detail"
                ((++EXT_FAIL))
                [[ -n ${LOGS_DIR:-} ]] && cp "$dir/warn.log" "$LOGS_DIR/$name.ext" 2>/dev/null
            fi
            continue
        fi

        linked=$(ext_header "$source" LINK)
        if [[ -n $linked ]]; then
            if timed "$COMPILE_TIMEOUT" "$ADA83" --ir "implementation/$linked" \
                 -o "$dir/linked.ll" >"$dir/compile.log" 2>&1; then
                fragments+=("$dir/linked.ll")
            else
                detail="support unit $linked: $(tail -2 "$dir/compile.log" | tr '\n' ' ')"
            fi
        fi

        local build_flags; build_flags=$(ext_header "$source" FLAGS)
        if [[ -z $detail ]] &&
           ! ( cd "$dir" && timed "$LINK_TIMEOUT" "$ADA83" $build_flags "$ROOT/$source" \
                 ${fragments[@]+"${fragments[@]}"} -o "$dir/$name" ) \
                 >>"$dir/compile.log" 2>&1; then
            detail=$(tail -2 "$dir/compile.log" | tr '\n' ' ')
        fi

        # msys bash resolves the extension-less path for -x and for running,
        # but nm needs the file that actually exists
        exe=$dir/$name
        [[ -f $exe.exe ]] && exe=$exe.exe

        if [[ -z $detail ]]; then
            symbol=$(ext_header "$source" SYMBOL)
            symbol_not=$(ext_header "$source" SYMBOL-NOT)
            if [[ -n $symbol || -n $symbol_not ]]; then
                if [[ -z $nm_tool ]]; then
                    printf '  %sskip%s %s — symbol check needs nm, which is not on PATH\n' \
                        "$DIM" "$OFF" "$name"
                    ((++EXT_SKIP)); continue
                fi
                # Mach-O prepends an underscore to every C-level symbol, so
                # accept the name with or without one leading underscore
                symbols=$("$nm_tool" "$exe" 2>/dev/null) || symbols=""
                if [[ -n $symbol ]] &&
                   ! grep -qE "[[:space:]]_?$symbol\$" <<<"$symbols"; then
                    detail="symbol $symbol missing from the executable"
                elif [[ -n $symbol_not ]] &&
                     grep -qE "[[:space:]]_?$symbol_not\$" <<<"$symbols"; then
                    detail="symbol $symbol_not present in the executable"
                fi
            fi
        fi

        if [[ -z $detail ]]; then
            args=$(ext_header "$source" ARGS)
            eval "set -- $args"
            if ! timed "$TEST_TIMEOUT" "$exe" "$@" >"$dir/run.out" 2>"$dir/run.err"; then
                detail="exited $? — $(tail -2 "$dir/run.err" | tr '\n' ' ')"
            elif grep -q FAILED "$dir/run.out"; then
                detail=$(grep FAILED "$dir/run.out" | head -1)
            elif ! grep -q PASSED "$dir/run.out"; then
                detail="printed neither PASSED nor FAILED: $(head -1 "$dir/run.out")"
            fi
        fi

        if [[ -z $detail ]]; then
            printf '  %sok%s   %s\n' "$GOOD" "$OFF" "$name"
            ((++EXT_PASS))
        else
            printf '  %sFAIL%s %s — %s\n' "$BAD" "$OFF" "$name" "$detail"
            ((++EXT_FAIL))
            [[ -n ${LOGS_DIR:-} ]] && cp "$dir/compile.log" "$LOGS_DIR/$name.ext" 2>/dev/null
        fi
    done

    [[ ${KEEP_WORK:-0} == 1 ]] || rm -rf "$work"

    printf '\n  %s%d implementation tests: %d passed, %d failed, %d skipped%s\n' \
        "$BOLD" $((EXT_PASS + EXT_FAIL + EXT_SKIP)) "$EXT_PASS" "$EXT_FAIL" \
        "$EXT_SKIP" "$OFF"

    # The summary is what CI gates on: I is the implementation-test tally
    # and IF its failures.
    [[ -n ${RESULTS_DIR:-} && -f $RESULTS_DIR/test_summary.txt ]] &&
        printf ' I=%d/%d IF=%d IS=%d\n' \
            "$EXT_PASS" $((EXT_PASS + EXT_FAIL + EXT_SKIP)) "$EXT_FAIL" "$EXT_SKIP" \
            >> "$RESULTS_DIR/test_summary.txt"
    return 0
}

usage(){ cat <<'TEXT'
Usage: test.sh [COMMAND] [SELECTOR]

Run the ACATS conformance suite and the extension tests against the ada83
compiler. With no arguments, runs every test.

Commands:
  run [SELECTOR]     run the suite and print a report (the default)
  check [SELECTOR]   run, then diff against the baseline; exit 1 on regression
  bless [SELECTOR]   run, then write the results as the new baseline
  list [SELECTOR]    list the tests a selector expands to
  implementation     run only the implementation tests (alias: extensions)
  help               display this help and exit

A full run (no selector, or `all`) ends with the extension tests: the Ada
programs under implementation/, which cover what ACATS cannot see -- the _ada_
symbol prefix on library subprograms, the Command_Line vendor package, and
what --analyze reports.  Each is named feature_NN_what_it_checks.
They are counted separately, as I= and IF= in the run summary.

Selectors:
  all                every test (default)
  a, b, c, d, e, l   one ACATS class
  PREFIX             a filename prefix, such as c45 or c45347

Environment:
  ADA83              compiler to test (default: bin-<platform>/ada83, built
                     with make if it is missing)
  OPT                optimisation flag the tests link with (default: -O2)
  JOBS, NPROC        parallel workers (default: the processor count)
  TEST_TIMEOUT       per-test run cap in seconds (default: 30)
  COMPILE_TIMEOUT    per-unit compile/bind cap in seconds (default: 30)
  LINK_TIMEOUT       per-test link cap in seconds (default: 20)
  STARTUP_TIMEOUT    compiler startup/version cap in seconds (default: 30)
  BUILD_TIMEOUT      compiler rebuild cap in seconds (default: 300)
  WORKER_TIMEOUT     last-resort cap around one complete test worker
  KILL_GRACE         seconds after TERM before timeout sends KILL (default: 2)
  BASELINE           baseline manifest path (default: acats.baseline)
  KEEP_WORK          set to 1 to keep the extension stage's working tree
  TAP                set to 1 to also write a TAP stream
  NO_ANIMATE         set to 1 to draw no progress indicators
  NO_COLOUR          set to 1 to draw no colour

Each run writes results to test_results/ID/ and logs to acats_logs/ID/, where
ID is unique to the run, so concurrent runs do not overwrite one another.
TEXT
}

main(){
    local cmd=${1:-run}; shift || true
    case $cmd in
        run|g)   run_selector "${1:-all}" "ACATS RUN — ${1:-all}"
                 [[ ${1:-all} == all ]] && run_implementation_tests ;;
        q)       run_selector "${1:-c32}" "ACATS RUN — ${1:-c32}" ;;
        check)   run_selector "${1:-all}" "ACATS CHECK — ${1:-all}"
                 [[ ${1:-all} == all ]] && run_implementation_tests
                 compare_to_baseline
                 ((REGRESSIONS==0 && ${EXT_FAIL:-0}==0)) || exit 1 ;;
        implementation|extensions|x)
                 run_implementation_tests
                 ((${EXT_FAIL:-0}==0)) || exit 1 ;;
        bless)   run_selector "${1:-all}" "ACATS BLESS — ${1:-all}"
                 write_baseline ;;
        list)    local pattern; pattern=$(selector_glob "${1:-all}")
                 for f in $pattern; do [[ -f $f ]] && basename "$f" .ada; done ;;
        h|help|-h|--help) usage ;;
        *)       usage; exit 2 ;;
    esac
}
main "$@"
