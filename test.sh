#!/usr/bin/env bash
set -euo pipefail

SELF=${0##*/}
HERE=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)

NO_ANIMATE=${NO_ANIMATE:-0} NO_COLOUR=${NO_COLOUR:-0} KEEP_WORK=${KEEP_WORK:-0}
SLOWEST=${SLOWEST:-12}

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
reap(){ kill "$1" 2>/dev/null || true; wait "$1" 2>/dev/null || true; }

pulse_stop(){ [ -n "$PULSE" ] && { reap "$PULSE"; PULSE=''; clear_line; }; return 0; }

heading(){
    printf '\n  %s%s%s\n' "$BOLD" "$1" "$OFF"
    [ $# -gt 1 ] && printf '  %s%s%s\n' "$DIM" "$2" "$OFF"
    printf '\n'
}
rule(){ printf '  %s%s%s\n' "$DIM" "────────────────────────────────────────────────────────────────" "$OFF"; }
die(){ pulse_stop; show_cursor; echo "$SELF: $*" >&2; exit 1; }

pct(){ ((${2:-0}>0)) && printf %d "$((100*$1/$2))" || printf 0; }

pm(){ case $1 in x) printf '%14s' 'x' ;; *) printf '%8s ±%-5s' "$1" "$2" ;; esac; }
spread(){ case $1 in x|'') printf '%6s' '' ;; *) awk -v r="$1" 'BEGIN{printf (r>=5)?"  !%3.0f%%":"  ±%3.0f%%", r}' ;; esac; }
ratio(){ case "$1$2" in *x*) printf '%7s' '-' ;; *) awk -v a="$1" -v b="$2" 'BEGIN{if(b+0==0)printf "%7s","-";else printf "%7.2f",a/b}' ;; esac; }
change(){ case "$1$2" in *x*) printf '%9s' '-' ;; *) awk -v n="$1" -v o="$2" 'BEGIN{if(o+0==0)printf "%9s","-";else printf "%+8.1f%%",(n-o)*100/o}' ;; esac; }

verdict(){
    case "$1$2" in *x*) return ;; esac
    local w; w=$(awk -v n="$1" -v o="$2" -v a="$3" -v b="$4" 'BEGIN{
        c=(n-o)*100/o; e=(a>b?a:b); print (c<-e)?"faster":((c>e)?"slower":"level") }')
    case $w in faster) printf '%sfaster%s' "$GOOD" "$OFF" ;;
               slower) printf '%sslower%s' "$BAD" "$OFF" ;;
               *)      printf '%slevel%s'  "$DIM" "$OFF" ;; esac
}

speedup(){ awk -v a="$1" -v g="$2" 'BEGIN{
    if (a+0<=0||g+0<=0) exit
    if (g>a) printf "%.1fx faster", g/a; else if (a>g) printf "%.1fx slower", a/g; else printf "level" }'; }

stats(){
    awk '{ v[++n] = $1 + 0 }
         END{ if (n == 0) { printf "x x 0"; exit }
              for (i = 1; i <= n; i++)
                  for (j = i + 1; j <= n; j++)
                      if (v[j] < v[i]) { h = v[i]; v[i] = v[j]; v[j] = h }
              med = (n % 2) ? v[(n + 1) / 2] : (v[n / 2] + v[n / 2 + 1]) / 2
              for (i = 1; i <= n; i++) { d[i] = v[i] - med; if (d[i] < 0) d[i] = -d[i] }
              for (i = 1; i <= n; i++)
                  for (j = i + 1; j <= n; j++)
                      if (d[j] < d[i]) { h = d[i]; d[i] = d[j]; d[j] = h }
              mad = (n % 2) ? d[(n + 1) / 2] : (d[n / 2] + d[n / 2 + 1]) / 2
              printf "%.3f %.3f %d", med, mad, n }' "$1"
}

rate_bar(){
    local label=$1 passed=$2 total=$3 rate colour
    rate=$(pct "$passed" "$total")
    if ((passed == total)); then colour=$GOOD; else colour=$BAD; fi
    printf '  %-22s %s%s%s %5s%%  %s/%s\n' "$label" \
        "$colour" "$(bar "$(scaled "$rate" 100 30)" 30)" "$OFF" "$rate" "$passed" "$total"
}

loadavg(){
    if [ -r /proc/loadavg ]
    then cut -d' ' -f1 </proc/loadavg
    else uptime 2>/dev/null | sed 's/.*average[s]*: *//;s/[, ].*//'; fi
}

LOAD_PEAK=0 LOAD_LAST=0 LOAD_WATCHED=0 LOAD_SAMPLES=''
load_watch_start(){
    [ -n "$LOAD_SAMPLES" ] || return 0
    ( while :; do loadavg; sleep 2; done > "$LOAD_SAMPLES" 2>/dev/null ) &
    LOAD_WATCH=$!
}
# Not a command substitution: a subshell would lose both values.
load_watch_stop(){
    local pk
    LOAD_LAST=0
    [ -n "${LOAD_WATCH:-}" ] || return 0
    reap "$LOAD_WATCH"; LOAD_WATCH=''
    pk=$(sort -g "$LOAD_SAMPLES" 2>/dev/null | tail -1); [ -n "$pk" ] || pk=0
    LOAD_LAST=$pk LOAD_WATCHED=1
    awk -v p="$pk" -v m="$LOAD_PEAK" 'BEGIN{exit !(p+0 > m+0)}' && LOAD_PEAK=$pk
    return 0
}

cleanup(){
    pulse_stop
    show_cursor
    [ -n "${REPORT_LL:-}" ] && rm -f "$REPORT_LL" "${REPORT_LL%.ll}.ali"
    [ -n "${BENCH_WORK:-}" ] && [ "$KEEP_WORK" != 1 ] && rm -rf "$BENCH_WORK"
    return 0
}
trap cleanup EXIT
trap 'show_cursor; exit 130' INT

case $(uname -s 2>/dev/null) in
    Darwin)                 HOST_TARGET=macos ;;
    MINGW*|MSYS*|CYGWIN*)   HOST_TARGET=windows ;;
    *)                      HOST_TARGET=linux ;;
esac

# $2 names the directory to take, so that a tree holding one suite but not
# the other does not have to be unpacked whole.  -o is deliberate for the
# suites tests.zip owns; the suites git tracks are not in the archive at
# all, so unpacking can never overwrite a tracked file with a stale copy.
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

unpack_suite(){
    local root=$1 suite=$2 rc=0
    [ -d "$root/$suite" ] && return 0
    [ -f "$root/tests.zip" ] || return 1
    pulse "unpacking $suite"
    ( cd "$root" && unpack tests.zip "$suite" ) || rc=$?
    pulse_stop
    [ "$rc" = 0 ] && [ -d "$root/$suite" ]
}

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

timed(){
    local secs=$1
    shift
    "$TIMEOUT" -k "$KILL_GRACE" "$secs" "$@"
}

now_ms(){
    local stamp
    stamp=$(date +%s%3N 2>/dev/null)
    [[ $stamp =~ ^[0-9]+$ ]] || stamp=$(( $(date +%s) * 1000 ))
    printf '%s' "$stamp"
}

elapsed(){
    local ms=$(( $(now_ms) - START_MS ))
    printf '%d.%03d' $((ms / 1000)) $((ms % 1000))
}

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
        echo "$SELF: no 'make' to build the compiler with" >&2
        echo "         Windows: run make.bat, then start this script again" >&2
        return 1
    fi
}

acats_setup(){
    NPROC=${JOBS:-${NPROC:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}}
    TEST_TIMEOUT=${TEST_TIMEOUT:-30}
    COMPILE_TIMEOUT=${COMPILE_TIMEOUT:-30}
    LINK_TIMEOUT=${LINK_TIMEOUT:-20}
    STARTUP_TIMEOUT=${STARTUP_TIMEOUT:-30}
    BUILD_TIMEOUT=${BUILD_TIMEOUT:-300}
    KILL_GRACE=${KILL_GRACE:-2}
    WORKER_TIMEOUT=${WORKER_TIMEOUT:-$((8 * COMPILE_TIMEOUT + 4 * LINK_TIMEOUT + 2 * TEST_TIMEOUT + 30))}
    BASELINE=${BASELINE:-acats.baseline}
    OPT=${OPT:--O2}

    TIMEOUT=$(find_timeout) || {
        echo "$SELF: no usable 'timeout' command found" >&2
        echo "         macOS:   brew install coreutils" >&2
        echo "         Windows: run this under the bash that comes with Git for" >&2
        echo "                  Windows, whose /usr/bin/timeout comes first on PATH" >&2
        exit 1
    }
    export TIMEOUT
    export -f timed
    export KILL_GRACE

    START_MS=$(now_ms)
    mkdir -p test_results acats_logs

    local suite
    for suite in acats extensions project debug acats-bonus; do
        unpack_suite "$PWD" "$suite" || die \
            "$suite is missing and tests.zip does not carry it; it is tracked in git -- restore it with: git checkout -- $suite"
    done

    ADA83=$(find_ada83) || ADA83=""
    if [[ -z $ADA83 ]] || [[ ada83.c -nt $ADA83 ]]; then
        printf '  %srebuilding ada83%s\n' "$DIM" "$OFF"
        build_ada83 || die "compiler build failed"
        ADA83=$(find_ada83) || die "no ada83 executable after building"
    fi
    [[ $ADA83 == /* || $ADA83 == ?:[/\\]* ]] || ADA83=$PWD/${ADA83#./}
    export ADA83

    export REPORT_LL="${TMPDIR:-/tmp}/ada83-report-$$.ll"
    timed "$COMPILE_TIMEOUT" "$ADA83" -ada83 --ir acats/report.adb -o "$REPORT_LL" >/dev/null 2>&1 || \
        die "cannot compile acats/report.adb"

    ROOT=$PWD
    export ROOT
    export -f run_one run_one_timed gather_files compile_set run_in_lib \
              link_program report_link_failure run_continuity_creators pct now_ms
    export START_MS TEST_TIMEOUT LINK_TIMEOUT COMPILE_TIMEOUT STARTUP_TIMEOUT \
           BUILD_TIMEOUT WORKER_TIMEOUT OPT
}

gather_files(){
    local f=$1 n=$2
    COMPILE_FILES=("$f")
    if [[ $n =~ ^(.*[a-z])([0-9])m$ ]]; then
        local base=${BASH_REMATCH[1]} g
        local -a family=()
        for g in "acats/$base"[0-9].ada "acats/$base"[0-9]m.ada "acats/$base"[a-z].ada; do
            [[ -f $g ]] && family+=("$g")
        done
        if ((${#family[@]} > 1)); then
            mapfile -t family < <(printf '%s\n' "${family[@]}" | sort)
            COMPILE_FILES=("${family[@]}")
        fi
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
        if ! timed "$COMPILE_TIMEOUT" "$ADA83" -ada83 --ir "$part" -o "$lib/$pn.ll" >/dev/null 2>"$LOGS_DIR/$n.err"; then
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
        local -a frag_units=() units=()
        local -A last_provider=()
        for ((i = 0; i < ${#LINK_FRAGMENTS[@]}; i++)); do
            frag=${LINK_FRAGMENTS[i]}
            frag_units[i]=$(grep '^U ' "${frag%.ll}.ali" 2>/dev/null \
                            | awk '{print $2}' | sed 's/%.*//' | tr '[:upper:]' '[:lower:]')
            mapfile -t units <<<"${frag_units[i]}"
            for unit in "${units[@]}"; do [[ -n $unit ]] && last_provider[$unit]=$i; done
        done
        for ((i = 0; i < ${#LINK_FRAGMENTS[@]}; i++)); do
            frag=${LINK_FRAGMENTS[i]}
            local in_main=0 current=0
            [[ -n ${frag_units[i]//[$' \n']} ]] || current=1
            mapfile -t units <<<"${frag_units[i]}"
            for unit in "${units[@]}"; do
                [[ -n $unit ]] || continue
                u_current=1
                anc=$unit
                while :; do
                    p=${last_provider[$anc]:-$i}
                    ((p > i)) && u_current=0
                    [[ $anc == *.* ]] || break
                    anc=${anc%.*}
                done
                ((u_current)) && current=1
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
               | grep -oiE '"ce[0-9a-z]+"' | tr -d '"' | tr '[:upper:]' '[:lower:]' | sort -u); do
        [[ $c == "$self" || ! -f acats/$c.ada ]] && continue
        timed "$COMPILE_TIMEOUT" "$ADA83" -ada83 --ir "acats/$c.ada" -o "$lib/$c.ll" \
            >/dev/null 2>&1 || continue
        timed "$LINK_TIMEOUT" "$ADA83" "$OPT" "$lib/$c.ll" "$REPORT_LL" \
            -o "$lib/$c.bin" >/dev/null 2>&1 || continue
        ( cd "$lib" && timed "$TEST_TIMEOUT" "./$c.bin" ) >/dev/null 2>&1 || true
    done
}

run_one(){
    local f=$1 n q=${1##*/}
    n=$(basename "$1" .ada); q=${q:0:1}
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
            echo "c skip $n COMPILE[$COMPILE_FAILED]:$(head -1 "$LOGS_DIR/$n.err" 2>/dev/null|cut -c1-50)"
            return
        fi
        if [[ -n $BIND_FAILED ]]; then
            echo "c fail $n OBSOLETE:$(head -1 "$LOGS_DIR/$n.bind" 2>/dev/null|cut -c1-50)"
            return
        fi
        if ! link_program "$n"; then
            report_link_failure c "$n"
            return
        fi
        run_continuity_creators "$n" "$RESULTS_DIR/$n.lib"
        local rc=0
        run_in_lib "$TEST_TIMEOUT" "$n" > "$LOGS_DIR/$n.out" 2>&1 || rc=$?
        if ((rc==124 || rc==137)); then
            echo "c fail $n TIMEOUT:exceeded_${TEST_TIMEOUT}s"
            return
        fi
        if ((rc==0)); then
            if grep -q PASSED "$LOGS_DIR/$n.out" 2>/dev/null; then
                echo "c pass $n PASSED"
            elif grep -q '^NOT APPLICABLE:' "$LOGS_DIR/$n.out" 2>/dev/null; then
                echo "c skip $n N/A:$(grep -o '^NOT APPLICABLE:.*' "$LOGS_DIR/$n.out"|head -1|cut -c1-40)"
            elif grep -q FAILED "$LOGS_DIR/$n.out" 2>/dev/null; then
                echo "c fail $n FAILED:$(grep FAILED "$LOGS_DIR/$n.out"|head -1|cut -c1-50)"
            else
                echo "c fail $n NO_REPORT:no_PASSED/FAILED_in_output"
            fi
        else
            echo "c fail $n RUNTIME:exit_${rc}"
        fi
        ;;
    a)
        if ! compile_set "$n"; then
            echo "a skip $n COMPILE[$COMPILE_FAILED]:$(head -1 "$LOGS_DIR/$n.err" 2>/dev/null|cut -c1-50)"; return; fi
        if [[ -n $BIND_FAILED ]]; then
            echo "a fail $n OBSOLETE:$(head -1 "$LOGS_DIR/$n.bind" 2>/dev/null|cut -c1-50)"; return; fi
        if ! link_program "$n"; then
            report_link_failure a "$n"
            return; fi
        local rc=0
        run_in_lib "$TEST_TIMEOUT" "$n" > "$LOGS_DIR/$n.out" 2>&1 || rc=$?
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
        local part pn i hits=0 rejected="" e v l m _
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
            if timed "$COMPILE_TIMEOUT" "$ADA83" -ada83 --ir "$part" -o "$lib/${pn%.ada}.ll" \
                 >/dev/null 2>"$LOGS_DIR/$n.$pn.err"; then :; else
                rejected=yes
            fi
            while IFS=: read -r file m _; do
                actual+=("$(basename "$file"):$m")
            done < <(grep "^[^:]*:[0-9]*:[0-9]*: " "$LOGS_DIR/$n.$pn.err")
        done
        if [[ -z $rejected ]]; then
            echo "b fail $n WRONG_ACCEPT:compiled_when_should_reject"
        else
            local ef el ew vf vl xe p
            for e in ${expected[@]+"${expected[@]}"}; do
                ew=${e##*:}; ef=${e%%:*}
                el=${e#*:}; el=${el%%:*}
                for v in ${actual[@]+"${actual[@]}"}; do
                    vf=${v%:*}; vl=${v##*:}
                    [[ $vf == "$ef" ]] && ((vl>=el-ew&&vl<=el+ew)) && { ((++hits)); break; }
                done
            done
            xe=${#expected[@]}
            p=$(pct $hits "$xe")
            ((p>=90)) && echo "b pass $n REJECTED:${hits}/${xe}_errors_(${p}%)" \
                      || echo "b fail $n LOW_COVERAGE:${hits}/${xe}_errors_(${p}%)"
        fi
        ;;
    d)
        if ! compile_set "$n"; then
            echo "d skip $n COMPILE[$COMPILE_FAILED]:$(head -1 "$LOGS_DIR/$n.err" 2>/dev/null|cut -c1-50)"; return; fi
        if [[ -n $BIND_FAILED ]]; then
            echo "d fail $n OBSOLETE:$(head -1 "$LOGS_DIR/$n.bind" 2>/dev/null|cut -c1-50)"; return; fi
        if ! link_program "$n"; then
            report_link_failure d "$n"
            return; fi
        if run_in_lib "$TEST_TIMEOUT" "$n" > "$LOGS_DIR/$n.out" 2>&1 && grep -q PASSED "$LOGS_DIR/$n.out"; then
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
            e_reject "$n" COMPILE "$(head -1 "$LOGS_DIR/$n.err" 2>/dev/null|cut -c1-50)"; return; fi
        if [[ -n $BIND_FAILED ]]; then
            e_reject "$n" BIND "$(head -1 "$LOGS_DIR/$n.bind" 2>/dev/null|cut -c1-50)"; return; fi
        if ! link_program "$n"; then
            report_link_failure e "$n"
            return; fi
        run_in_lib "$TEST_TIMEOUT" "$n" > "$LOGS_DIR/$n.out" 2>&1 || true
        if grep -q "TENTATIVELY PASSED" "$LOGS_DIR/$n.out" 2>/dev/null; then
            echo "e pass $n INSPECT:requires_manual_verification"
        elif grep -q PASSED "$LOGS_DIR/$n.out" 2>/dev/null; then
            echo "e pass $n PASSED"
        else
            echo "e fail $n FAILED"
        fi
        ;;
    l)
        if compile_set "$n"; then
            if [[ -n $BIND_FAILED ]]; then
                echo "l pass $n BIND_REJECT:$(head -1 "$LOGS_DIR/$n.bind" 2>/dev/null|cut -c1-40)"
                return
            fi
            if link_program "$n"; then
                if run_in_lib 1 "$n" > "$LOGS_DIR/$n.out" 2>&1; then
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
            echo "l pass $n COMPILE_REJECT:$(head -1 "$LOGS_DIR/$n.err" 2>/dev/null|cut -c1-40)"
        fi
        ;;
    f) ;;
    *) echo "? skip $n UNKNOWN:unrecognized_class" ;;
    esac
}

run_one_timed(){
    local f=$1 n q started
    n=$(basename "$1" .ada)
    q=${f##*/}; q=${q:0:1}; q=${q,,}
    local out rc=0 graded=1
    started=$(now_ms)
    out=$(timed "$WORKER_TIMEOUT" bash -c "run_one \"\$1\"" _ "$f" 2>/dev/null) || rc=$?
    if ((rc==124 || rc==137)); then
        echo "$q fail $n TIMEOUT:worker_exceeded_${WORKER_TIMEOUT}s"
    elif ((rc != 0)); then
        echo "$q fail $n HARNESS:worker_exit_${rc}"
    elif [[ -n $out ]]; then
        echo "$out"
    else
        graded=0
    fi
    if [[ -n ${PROGRESS_FILE:-} ]]; then
        printf '%s\t%s\t%s\n' "$(( $(now_ms) - started ))" "$graded" "$n" >> "$PROGRESS_FILE"
    fi
    return 0
}

CLASS_ROWS=(a "A  Acceptance"  b "B  Illegality"  c "C  Executable"
            d "D  Numerics"    e "E  Inspection"  l "L  Post-compilation")

tally_results(){
    local results_file=$1
    local -A C=([a]=0 [b]=0 [c]=0 [d]=0 [e]=0 [l]=0
                [fa]=0 [fb]=0 [fc]=0 [fd]=0 [fe]=0 [fl]=0
                [sa]=0 [sb]=0 [sc]=0 [sd]=0 [se]=0 [sl]=0
                [ta]=0 [tb]=0 [tc]=0 [td]=0 [te]=0 [tl]=0
                [f]=0 [s]=0 [z]=0)
    : > "$RESULTS_TSV"

    local cls result name detail k i
    while read -r cls result name detail; do
        [[ -z $cls ]] && continue
        k=${cls,,}
        printf '%s\t%s\t%s\t%s\n' "$name" "$k" "$result" "$detail" >> "$RESULTS_TSV"
        C[z]=$(( C[z] + 1 ))
        C[t$k]=$(( ${C[t$k]:-0} + 1 ))
        case $result in
            pass) C[$k]=$(( ${C[$k]:-0} + 1 )) ;;
            fail) C[f]=$(( C[f] + 1 )); C[f$k]=$(( ${C[f$k]:-0} + 1 )) ;;
            skip) C[s]=$(( C[s] + 1 )); C[s$k]=$(( ${C[s$k]:-0} + 1 )) ;;
        esac
    done < "$results_file"

    local passing=$(( C[a] + C[b] + C[c] + C[d] + C[e] + C[l] ))

    heading "RESULTS"
    printf '  %-22s %6s %6s %6s %6s %7s\n' class pass fail skip total rate
    rule
    for ((i = 0; i < ${#CLASS_ROWS[@]}; i += 2)); do
        k=${CLASS_ROWS[i]}
        ((${C[t$k]} > 0)) && printf '  %-22s %6d %6d %6d %6d %6d%%\n' "${CLASS_ROWS[i+1]}" \
            "${C[$k]}" "${C[f$k]}" "${C[s$k]}" "${C[t$k]}" "$(pct "${C[$k]}" "${C[t$k]}")"
    done
    rule
    printf '  %s%-22s %6d %6d %6d %6d %6d%%%s\n' "$BOLD" TOTAL "$passing" "${C[f]}" "${C[s]}" \
        "${C[z]}" "$(pct "$passing" "${C[z]}")" "$OFF"
    printf '\n  %selapsed%s %ss over %d tests with %s workers, %s\n' \
        "$DIM" "$OFF" "$(elapsed)" "${C[z]}" "$NPROC" "$(date '+%Y-%m-%d %H:%M:%S')"
    [ "$LOAD_WATCHED" = 1 ] && printf '  %sload%s    %s now, %s at its worst while running\n' \
        "$DIM" "$OFF" "$(loadavg)" "$LOAD_PEAK"

    heading "PASS RATE BY CLASS"
    for ((i = 0; i < ${#CLASS_ROWS[@]}; i += 2)); do
        k=${CLASS_ROWS[i]}
        ((${C[t$k]} > 0)) && rate_bar "${CLASS_ROWS[i+1]}" "${C[$k]}" "${C[t$k]}"
    done
    rule
    rate_bar TOTAL "$passing" "${C[z]}"

    printf "A=%d B=%d C=%d D=%d E=%d L=%d F=%d S=%d T=%d/%d (%d%%)\n" \
        "${C[a]}" "${C[b]}" "${C[c]}" "${C[d]}" "${C[e]}" "${C[l]}" "${C[f]}" "${C[s]}" \
        "$passing" "${C[z]}" "$(pct "$passing" "${C[z]}")" > "$RESULTS_DIR/test_summary.txt"

    if [[ ${TAP:-0} == 1 ]]; then
        { echo "TAP version 13"; local j=0 _k
          while IFS=$'\t' read -r name _k result detail; do ((++j))
            case $result in
              pass) echo "ok $j $name" ;;
              skip) echo "ok $j $name # SKIP ${detail}" ;;
              *)    echo "not ok $j $name # ${detail}" ;;
            esac
          done < "$RESULTS_TSV"
          echo "1..$j"; } > "$RESULTS_DIR/results.tap"
    fi
}

report_slowest(){
    local timings=$1 pairs secs med mad n peak t name
    [[ -s $timings ]] || return 0
    pairs=$(mktemp); secs=$(mktemp)
    awk -F'\t' '$2 == 1 { printf "%.3f\t%s\n", $1/1000, $3 }' "$timings" | sort -rn > "$pairs"
    [[ -s $pairs ]] || { rm -f "$pairs" "$secs"; return 0; }
    cut -f1 "$pairs" > "$secs"
    read -r med mad n <<<"$(stats "$secs")"
    peak=$(head -1 "$secs")
    heading "SLOWEST TESTS" "wall time of one worker, compile through run"
    while IFS=$'\t' read -r t name; do
        printf '  %-18s %7ss  %s %s%sx%s\n' "$name" "$t" "$(bar "$(scaled "$t" "$peak" 24)" 24)" \
            "$DIM" "$(ratio "$t" "$med")" "$OFF"
    done < <(head -n "$SLOWEST" "$pairs")
    printf '\n  %sper test%s %s  median ± MAD over %s tests\n' "$DIM" "$OFF" "$(pm "$med" "$mad")" "$n"
    rm -f "$pairs" "$secs"
}

compare_to_baseline(){
    REGRESSIONS=0
    if [[ ! -f $BASELINE ]]; then
        printf '\n  %sno baseline at %s — run %s to create one%s\n' \
            "$DIM" "$BASELINE" "\`$SELF bless\`" "$OFF"
        return 0
    fi
    local -A BL RES
    local name status _c _d _rest
    while IFS=$'\t' read -r name status _rest; do [[ -n $name ]] && BL[$name]=$status; done < "$BASELINE"
    while IFS=$'\t' read -r name _c status _d; do [[ -n $name ]] && RES[$name]=$status; done < "$RESULTS_TSV"

    local -a regr=() prog=() chg=() new=() miss=()
    local k b r shared=0 was=0 now=0
    for k in "${!RES[@]}"; do
        b=${BL[$k]:-__absent__}; r=${RES[$k]}
        if [[ $b != __absent__ ]]; then
            shared=$((shared + 1))
            [[ $b == pass ]] && was=$((was + 1))
            [[ $r == pass ]] && now=$((now + 1))
        fi
        if   [[ $b == __absent__ ]]; then new+=("$k $r")
        elif [[ $b == "$r" ]];      then :
        elif [[ $b == pass ]];      then regr+=("$k $b->$r")
        elif [[ $r == pass ]];      then prog+=("$k $b->$r")
        else                             chg+=("$k $b->$r"); fi
    done
    for k in "${!BL[@]}"; do [[ -z ${RES[$k]:-} ]] && miss+=("$k ${BL[$k]}"); done
    REGRESSIONS=${#regr[@]}

    heading "BASELINE DIFF" "$BASELINE"
    emit_group(){ local tag=$1 colour=$2; shift 2; (($#)) || return 0
                  printf '  %s%s (%d)%s\n' "$colour" "$tag" "$#" "$OFF"
                  printf '    %s\n' "$@" | sort; printf '\n'; }
    emit_group "REGRESSIONS"  "$BAD"  ${regr[@]+"${regr[@]}"}
    emit_group "PROGRESSIONS" "$GOOD" ${prog[@]+"${prog[@]}"}
    emit_group "CHANGED"      "$BOLD" ${chg[@]+"${chg[@]}"}
    emit_group "NEW"          "$BOLD" ${new[@]+"${new[@]}"}
    emit_group "MISSING"      "$BOLD" ${miss[@]+"${miss[@]}"}
    ((shared > 0)) && printf '  %spassing%s  %d of %d shared, was %d, %s\n' \
        "$DIM" "$OFF" "$now" "$shared" "$was" "$(change "$now" "$was")"
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

selector_prefix(){
    case ${1:-all} in
        all)                      printf '' ;;
        a|b|c|d|e|l|A|B|C|D|E|L)  printf '%s' "${1,,}" ;;
        *)                        printf '%s' "$1" ;;
    esac
}

selector_files(){
    local prefix f
    prefix=$(selector_prefix "${1:-all}")
    SELECTED=()
    for f in acats/"$prefix"*.ada; do [[ -f $f ]] && SELECTED+=("$f"); done
    return 0
}

run_selector(){
    local sel=${1:-all} title=$2
    local -a SELECTED=()
    selector_files "$sel"
    local total=${#SELECTED[@]}
    ((total > 0)) || die "selector '$sel' matched no tests"

    local run_id; run_id="${sel}-$(date +%Y%m%d-%H%M%S)-$$"
    export RESULTS_DIR="test_results/${run_id}"
    export LOGS_DIR="acats_logs/${run_id}"
    export RESULTS_TSV="$RESULTS_DIR/results.tsv"
    mkdir -p "$RESULTS_DIR" "$LOGS_DIR"

    local listfile; listfile=$(mktemp)
    printf '%s\n' "${SELECTED[@]}" > "$listfile"

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
    LOAD_SAMPLES=$(mktemp); LOAD_PEAK=0
    hide_cursor
    load_watch_start
    local tmpfile; tmpfile=$(mktemp)
    local worker_errors="$LOGS_DIR/harness.err"
    local runner_status; runner_status=$(mktemp)
    : > "$worker_errors"
    (
        set +e
        xargs -P "$NPROC" -I{} bash -c "run_one_timed \"\$@\"" _ {} \
            < "$listfile" > "$tmpfile" 2>"$worker_errors"
        rc=$?
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
    load_watch_stop
    clear_line; show_cursor
    if ((runner_rc != 0)); then
        printf '  %swarning:%s worker launcher exited with status %d; see %s\n' \
            "$BAD" "$OFF" "$runner_rc" "$worker_errors" >&2
    fi
    sort -k3 "$tmpfile" > "${tmpfile}.sorted"
    tally_results "${tmpfile}.sorted"
    report_slowest "$PROGRESS_FILE"
    rm -f "$tmpfile" "${tmpfile}.sorted" "$listfile" "$PROGRESS_FILE" "$runner_status" "$LOAD_SAMPLES"
    LOAD_SAMPLES=''
}

ext_header(){ sed -n "s/^-- $2: //p" "$1" | tail -1; }

run_extension_tests(){
    EXT_PASS=0 EXT_FAIL=0 EXT_SKIP=0
    [[ -d extensions ]] || { printf '  %sno extensions/ directory%s\n' "$DIM" "$OFF"; return 0; }

    local nm_tool; nm_tool=$(command -v nm || command -v llvm-nm || true)
    local work; work=$(mktemp -d "${TMPDIR:-/tmp}/ada83-ext.XXXXXX")

    local -A is_support=()
    local source linked
    for source in extensions/*.ada; do
        linked=$(ext_header "$source" LINK)
        [[ -n $linked ]] && is_support[$linked]=1
    done

    heading "EXTENSIONS" "the _ada_ symbol prefix and Command_Line"

    local name dir exe symbol symbol_not symbols args detail
    local -a fragments argv
    for source in extensions/*.ada; do
        name=$(basename "$source" .ada)
        [[ -n ${is_support[$name.ada]:-} ]] && continue

        dir=$work/$name
        mkdir -p "$dir"
        fragments=()
        detail=""

        linked=$(ext_header "$source" LINK)
        if [[ -n $linked ]]; then
            if timed "$COMPILE_TIMEOUT" "$ADA83" --ir "extensions/$linked" \
                 -o "$dir/linked.ll" >"$dir/compile.log" 2>&1; then
                fragments+=("$dir/linked.ll")
            else
                detail="support unit $linked: $(tail -2 "$dir/compile.log" | tr '\n' ' ')"
            fi
        fi

        if [[ -z $detail ]] &&
           ! ( cd "$dir" && timed "$LINK_TIMEOUT" "$ADA83" "$ROOT/$source" \
                 ${fragments[@]+"${fragments[@]}"} -o "$dir/$name" ) \
                 >>"$dir/compile.log" 2>&1; then
            detail=$(tail -2 "$dir/compile.log" | tr '\n' ' ')
        fi

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
            argv=(); [[ -n $args ]] && eval "argv=($args)"
            if ! timed "$TEST_TIMEOUT" "$exe" ${argv[@]+"${argv[@]}"} \
                   >"$dir/run.out" 2>"$dir/run.err"; then
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

    [[ $KEEP_WORK == 1 ]] || rm -rf "$work"

    local ext_total=$((EXT_PASS + EXT_FAIL + EXT_SKIP))
    printf '\n  %s%d extension tests: %d passed, %d failed, %d skipped%s\n' \
        "$BOLD" "$ext_total" "$EXT_PASS" "$EXT_FAIL" "$EXT_SKIP" "$OFF"
    rate_bar EXTENSIONS "$EXT_PASS" "$ext_total"

    [[ -n ${RESULTS_DIR:-} && -f $RESULTS_DIR/test_summary.txt ]] &&
        printf ' X=%d/%d XF=%d XS=%d\n' "$EXT_PASS" "$ext_total" "$EXT_FAIL" "$EXT_SKIP" \
            >> "$RESULTS_DIR/test_summary.txt"
    return 0
}

# A case with a `.known' file beside it is expected to fail and is counted
# apart, so a recorded gap stays visible without reddening the suite -- and
# a `.known' case that starts passing is reported as a failure, because the
# marker has become the lie.
run_project_tests(){
    PROJ_PASS=0 PROJ_FAIL=0 PROJ_KNOWN=0
    [[ -d project ]] || { printf '  %sno project/ directory%s\n' "$DIM" "$OFF"; return 0; }

    heading "PROJECT FILES" "gpr and gpj build descriptions, read with -P ... -n"

    local suite line tail_line p f k
    for suite in gpj gpr build; do
        [[ -x project/$suite/run.sh || -f project/$suite/run.sh ]] || continue
        line=$(sh "project/$suite/run.sh" "$ADA83" 2>&1) || true
        printf '%s\n' "$line" | sed -n '/^\(FAILED\|KNOWN\)/s/^/  /p'
        tail_line=$(printf '%s\n' "$line" | tail -1)
        p=$(sed -n 's/.*[^0-9]\([0-9]\+\) passed.*/\1/p' <<<"$tail_line"); p=${p:-0}
        f=$(sed -n 's/.*[^0-9]\([0-9]\+\) failed.*/\1/p' <<<"$tail_line"); f=${f:-0}
        k=$(sed -n 's/.*[^0-9]\([0-9]\+\) known.*/\1/p'  <<<"$tail_line"); k=${k:-0}
        PROJ_PASS=$((PROJ_PASS + p)) PROJ_FAIL=$((PROJ_FAIL + f)) PROJ_KNOWN=$((PROJ_KNOWN + k))
        printf '  %-4s %s\n' "$suite" "$tail_line"
    done

    local proj_total=$((PROJ_PASS + PROJ_FAIL + PROJ_KNOWN))
    printf '\n  %s%d project tests: %d passed, %d failed, %d known%s\n' \
        "$BOLD" "$proj_total" "$PROJ_PASS" "$PROJ_FAIL" "$PROJ_KNOWN" "$OFF"
    rate_bar PROJECT "$PROJ_PASS" "$proj_total"

    [[ -n ${RESULTS_DIR:-} && -f $RESULTS_DIR/test_summary.txt ]] &&
        printf ' PJ=%d/%d PJF=%d PJK=%d\n' "$PROJ_PASS" "$proj_total" "$PROJ_FAIL" "$PROJ_KNOWN" \
            >> "$RESULTS_DIR/test_summary.txt"
    return 0
}

#  The acats-bonus suite: ACATS 4.2 tests for six post-Ada-83 features,
#  sanitized to Ada 83 plus the one feature under test.
#  Grading is per acats-bonus/STATUS: while a feature is `pending' its
#  failures are pending, not failures -- but a test that PASSES counts as a
#  pass either way, so the suite carries real signal from the first day.
run_bonus_tests(){
    BONUS_PASS=0 BONUS_FAIL=0 BONUS_PEND=0
    [[ -d acats-bonus ]] || { printf '  %sno acats-bonus/ directory%s\n' "$DIM" "$OFF"; return 0; }

    heading "ACATS BONUS" "post-Ada-83 features, now the default: protected types, expression functions, if/case expressions, pragma-equivalent aspects, generic formal defaults, subprogram pointers, controlled types, Ada 95 unit names, dot notation"

    local line tail_line
    line=$(sh acats-bonus/run.sh "$ADA83" 2>&1) || true
    printf '%s\n' "$line" | sed -n '/^FAILED/s/^/  /p'
    tail_line=$(printf '%s\n' "$line" | tail -1)
    BONUS_PASS=$(sed -n 's/.*[^0-9]\([0-9]\+\) passed.*/\1/p' <<<"$tail_line"); BONUS_PASS=${BONUS_PASS:-0}
    BONUS_FAIL=$(sed -n 's/.*[^0-9]\([0-9]\+\) failed.*/\1/p' <<<"$tail_line"); BONUS_FAIL=${BONUS_FAIL:-0}
    BONUS_PEND=$(sed -n 's/.*[^0-9]\([0-9]\+\) pending.*/\1/p' <<<"$tail_line"); BONUS_PEND=${BONUS_PEND:-0}

    local graded=$((BONUS_PASS + BONUS_FAIL))
    printf '\n  %s%s%s\n' "$BOLD" "$tail_line" "$OFF"
    rate_bar "ACATS BONUS" "$BONUS_PASS" "$((graded > 0 ? graded : 1))"
    return 0
}

# A case whose feature the driver still refuses is `pending', not failed --
# the debugging campaign's -g and --dump-* land on sibling branches -- and
# the moment a flag parses, its cases grade for real.
run_debug_tests(){
    DBG_PASS=0 DBG_FAIL=0 DBG_PEND=0
    [[ -d debug ]] || { printf '  %sno debug/ directory%s\n' "$DIM" "$OFF"; return 0; }

    heading "DEBUGGING" "-g DWARF and gdb sessions, --dump-tree and --dump-rep"

    local line tail_line p f d
    line=$(sh debug/run.sh "$ADA83" 2>&1) || true
    printf '%s\n' "$line" | sed -n '/^FAILED/s/^/  /p'
    tail_line=$(printf '%s\n' "$line" | tail -1)
    p=$(sed -n 's/.*[^0-9]\([0-9]\+\) passed.*/\1/p' <<<"$tail_line"); p=${p:-0}
    f=$(sed -n 's/.*[^0-9]\([0-9]\+\) failed.*/\1/p' <<<"$tail_line"); f=${f:-0}
    d=$(sed -n 's/.*[^0-9]\([0-9]\+\) pending.*/\1/p' <<<"$tail_line"); d=${d:-0}
    DBG_PASS=$p DBG_FAIL=$f DBG_PEND=$d

    local dbg_total=$((DBG_PASS + DBG_FAIL + DBG_PEND))
    printf '\n  %s%d debug tests: %d passed, %d failed, %d pending%s\n' \
        "$BOLD" "$dbg_total" "$DBG_PASS" "$DBG_FAIL" "$DBG_PEND" "$OFF"
    rate_bar DEBUGGING "$DBG_PASS" "$((DBG_PASS + DBG_FAIL))"

    [[ -n ${RESULTS_DIR:-} && -f $RESULTS_DIR/test_summary.txt ]] &&
        printf ' DBG=%d/%d DBGF=%d DBGP=%d\n' \
            "$DBG_PASS" "$((DBG_PASS + DBG_FAIL))" "$DBG_FAIL" "$DBG_PEND" \
            >> "$RESULTS_DIR/test_summary.txt"
    return 0
}

# The reproducers: every program a fix was landed with, under repro/, run
# in isolation and judged by the expectation lines in its own header --
# the conventions are at the top of repro/run.sh.
run_repro_tests(){
    REPRO_PASS=0 REPRO_FAIL=0
    [[ -f repro/run.sh ]] || { printf '  %sno repro/run.sh%s\n' "$DIM" "$OFF"; return 0; }

    heading "REPRODUCERS" "the program each fix was landed with, run in isolation and judged by its header"

    local line tail_line
    line=$(bash repro/run.sh "$ADA83" 2>&1) || true
    printf '%s\n' "$line" | sed -n '/^FAILED/s/^/  /p'
    tail_line=$(printf '%s\n' "$line" | tail -1)
    REPRO_PASS=$(sed -n 's/.*[^0-9]\([0-9]\+\) passed.*/\1/p' <<<"$tail_line"); REPRO_PASS=${REPRO_PASS:-0}
    REPRO_FAIL=$(sed -n 's/.*[^0-9]\([0-9]\+\) failed.*/\1/p' <<<"$tail_line"); REPRO_FAIL=${REPRO_FAIL:-0}

    local repro_total=$((REPRO_PASS + REPRO_FAIL))
    printf '\n  %s%s%s\n' "$BOLD" "$tail_line" "$OFF"
    rate_bar REPRODUCERS "$REPRO_PASS" "$((repro_total > 0 ? repro_total : 1))"

    [[ -n ${RESULTS_DIR:-} && -f $RESULTS_DIR/test_summary.txt ]] &&
        printf ' RP=%d/%d RPF=%d\n' "$REPRO_PASS" "$repro_total" "$REPRO_FAIL" \
            >> "$RESULTS_DIR/test_summary.txt"
    return 0
}

#  The last four measure the language extensions whose cost a test cannot
#  show: each is legal Ada 95 as well, so
#  GNAT compiles the same program as the reference.
ALL_PROGRAMS="sieve matmul lu recurse strings numerics checks exceptions memory tasking taskflood taskselect taskelse indirect monitor finalizer wraparound"

describe(){ case $1 in
    sieve)      echo "integer arrays, index checks" ;;
    matmul)     echo "dense float, nested loops" ;;
    lu)         echo "LU decomposition, float division" ;;
    recurse)    echo "call and return" ;;
    strings)    echo "slices and character work" ;;
    numerics)   echo "fixed point and 12-digit float" ;;
    checks)     echo "range and index checks in a hot loop" ;;
    exceptions) echo "raise, propagate, handle" ;;
    memory)     echo "allocation and deallocation" ;;
    tasking)    echo "rendezvous throughput" ;;
    taskflood)  echo "task creation and termination" ;;
    indirect)   echo "a call through an access-to-subprogram" ;;
    monitor)    echo "protected procedure, function and entry, uncontended" ;;
    finalizer)  echo "Initialize, Adjust and Finalize driven by the compiler" ;;
    wraparound) echo "modular arithmetic, a power of two and a modulus that is not" ;;
    taskselect) echo "selective wait with an else part (UNMEASURABLE — see taskelse)" ;;
    taskelse)   echo "selective wait with an else part, fixed poll count" ;;
esac; }

# Programs whose output must match GNAT's byte for byte. numerics is exempt:
# it accumulates a fixed point value forty million times, and Ada 83 lets an
# implementation choose its own `small` for such a type, so the two totals are
# both correct and different (132 against 720 here). Every other program is
# held to identical output, and a difference there is an error, not a footnote.
comparable(){ case $1 in numerics) return 1 ;; *) return 0 ;; esac; }

concurrent(){ case $1 in tasking|taskflood|taskselect|taskelse) return 0 ;; *) return 1 ;; esac; }

#  The feature half of each pair.
extended(){ case $1 in indirect|monitor|finalizer|wraparound) return 0 ;; *) return 1 ;; esac; }

measure(){
    local out='' t i
    for ((i = 0; i < WARMUP; i++)); do "$@" <"$seed" >/dev/null 2>&1; done
    for ((i = 0; i < REPEATS; i++)); do
        TIMEFORMAT=%R; t=$( { time "$@" <"$seed" >/dev/null 2>&1; } 2>&1 )
        case $t in ''|*[!0-9.]*) continue ;; esac
        out+=$t$'\n'
    done
    [ -n "$out" ] || { printf 'x 0'; return; }
    printf '%s' "$out" | awk '{v[NR]=$1; s+=$1}
        END{ n = NR
             if (n == 0) { printf "x 0"; exit }
             mean = s / n
             for (i = 1; i <= n; i++)
                 for (j = i + 1; j <= n; j++)
                     if (v[j] < v[i]) { hold = v[i]; v[i] = v[j]; v[j] = hold }
             mid = (n % 2) ? v[(n + 1) / 2] : (v[n / 2] + v[n / 2 + 1]) / 2
             for (i = 1; i <= n; i++) spread += (v[i] - mean) * (v[i] - mean)
             sd = (n > 1) ? sqrt(spread / (n - 1)) : 0
             printf "%.3f %.1f", mid, ((mean > 0) ? sd * 100 / mean : 0) }'
}

med(){ local -a v=(); read -r -a v <<<"$1"; printf '%s' "${v[0]:-x}"; }
rsd(){ local -a v=(); read -r -a v <<<"$1"; printf '%s' "${v[1]:-0}"; }

# Which CPUs, not how many: --cpuset-cpus=4,5 gives two CPUs numbered 4 and 5,
# where taskset -c 1 fails and the pinning the method rests on is silently lost.
allowed_cpus(){
    local list part a b i
    list=$(taskset -pc $$ 2>/dev/null | sed 's/.*: *//')
    if [ -z "$list" ]; then
        for ((i = 0; i < ncpu; i++)); do printf '%s\n' "$i"; done
        return
    fi
    local -a parts=()
    IFS=',' read -r -a parts <<<"$list"
    for part in ${parts[@]+"${parts[@]}"}; do
        case $part in
            *-*) a=${part%%-*}; b=${part##*-}
                 for ((i = a; i <= b; i++)); do printf '%s\n' "$i"; done ;;
            *)   printf '%s\n' "$part" ;;
        esac
    done
}

pinset(){
    if [ "$NO_PIN" != 1 ] && command -v taskset >/dev/null 2>&1 && taskset -c "$1" true 2>/dev/null
    then printf 'taskset\n-c\n%s\n' "$1"; fi
}

bench_pinning(){
    ncpu=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1)
    CPUS=(); mapfile -t CPUS < <(allowed_cpus)
    [ ${#CPUS[@]} -gt 0 ] || CPUS=(0)
    BENCH_CPU=${BENCH_CPU:-${CPUS[${#CPUS[@]}-1]}}
    # A polling `select ... else' on one core measures the scheduler, not the
    # code, so a program with tasks gets a pair.
    BENCH_CPUS_TASK=${BENCH_CPUS_TASK:-}
    if [ -z "$BENCH_CPUS_TASK" ]; then
        if [ ${#CPUS[@]} -ge 2 ]
        then BENCH_CPUS_TASK="${CPUS[${#CPUS[@]}-2]},${CPUS[${#CPUS[@]}-1]}"
        else BENCH_CPUS_TASK=$BENCH_CPU; fi
    fi

    PRIO=() PRIO_WHY=''
    if [ "$NO_PIN" != 1 ]; then
        if [ "$RT" = 1 ] && command -v chrt >/dev/null 2>&1 && chrt -f 50 true 2>/dev/null; then
            PRIO=(chrt -f 50); PRIO_WHY=', SCHED_FIFO 50'
        elif command -v nice >/dev/null 2>&1 && nice -n -5 true 2>/dev/null; then
            PRIO=(nice -n -5); PRIO_WHY=', nice -5'
        fi
    fi

    PIN_ONE=(); mapfile -t PIN_ONE < <(pinset "$BENCH_CPU")
    PIN_MANY=(); mapfile -t PIN_MANY < <(pinset "$BENCH_CPUS_TASK")
    PIN_ONE+=(${PRIO[@]+"${PRIO[@]}"}); PIN_MANY+=(${PRIO[@]+"${PRIO[@]}"})
    PIN=(${PIN_ONE[@]+"${PIN_ONE[@]}"})

    if [ ${#PIN_ONE[@]} -eq 0 ]; then PIN_WHY='unpinned'
    else PIN_WHY="pinned to cpu $BENCH_CPU, or cpus $BENCH_CPUS_TASK where the program has tasks${PRIO_WHY}"; fi
}

load_gate(){
    local l; l=$(loadavg); [ -n "$l" ] || return 0
    awk -v l="$l" -v m="$LOAD_MAX" 'BEGIN{exit !(l+0 > m+0)}' || return 0
    if [ "$FORCE" = 1 ]
    then printf '  %sload average is %s, above %s: measuring anyway (FORCE=1)%s\n' "$BAD" "$l" "$LOAD_MAX" "$OFF"
    else die "load average is $l, above LOAD_MAX=$LOAD_MAX; wait for the machine to settle or set FORCE=1"; fi
}

timed_run(){
    local t
    TIMEFORMAT=%R
    t=$( { time ${PIN[@]+"${PIN[@]}"} "$@" <"$seed" >/dev/null 2>&1; } 2>&1 )
    case $t in ''|*[!0-9.]*) return 1 ;; esac
    printf '%s\n' "$t"
}

measure_pair(){
    local a=$1 b=$2 i t
    : > "$BENCH_WORK/samples.a"; : > "$BENCH_WORK/samples.b"
    for ((i = 0; i < CODEGEN_WARMUP; i++)); do
        ${PIN[@]+"${PIN[@]}"} "$a" <"$seed" >/dev/null 2>&1
        [ -n "$b" ] && ${PIN[@]+"${PIN[@]}"} "$b" <"$seed" >/dev/null 2>&1
    done
    for ((i = 0; i < CODEGEN_REPEATS; i++)); do
        if [ -z "$b" ]; then
            t=$(timed_run "$a") && printf '%s\n' "$t" >> "$BENCH_WORK/samples.a"
        elif [ $((i % 2)) -eq 0 ]; then
            t=$(timed_run "$a") && printf '%s\n' "$t" >> "$BENCH_WORK/samples.a"
            t=$(timed_run "$b") && printf '%s\n' "$t" >> "$BENCH_WORK/samples.b"
        else
            t=$(timed_run "$b") && printf '%s\n' "$t" >> "$BENCH_WORK/samples.b"
            t=$(timed_run "$a") && printf '%s\n' "$t" >> "$BENCH_WORK/samples.a"
        fi
    done
    return 0
}

distinguishable(){
    awk -v a="$1" -v am="$2" -v g="$3" -v gm="$4" -v f="$FLOOR" 'BEGIN{
        if (a == "x" || g == "x") exit 1
        if (am + 0 < f + 0) am = f
        if (gm + 0 < f + 0) gm = f
        d = a - g; if (d < 0) d = -d
        exit !(d > am + gm) }'
}

peak_rss(){
    local o
    if o=$(/usr/bin/time -v "$@" <"$seed" 2>&1 >/dev/null); then
        printf '%s' "$o" | awk '/Maximum resident set size/{printf "%.1f", $NF/1024}'
    elif o=$(/usr/bin/time -l "$@" <"$seed" 2>&1 >/dev/null); then
        printf '%s' "$o" | awk '/maximum resident set size/{printf "%.1f", $1/1048576}'
    else printf 'x'; fi
}

install_gnat(){
    if [ "$HOST_TARGET" = windows ]; then
        gnat_note="no GNAT; get one with Alire (https://alire.ada.dev): alr toolchain --select"
        return 1
    fi
    local s='' c='' try; [ "$(id -u)" -eq 0 ] || s=sudo
    for try in "apt-get:$s apt-get install -y --no-install-recommends gnat" \
               "dnf:$s dnf install -y gcc-gnat" "pacman:$s pacman -S --noconfirm gcc-ada" \
               "zypper:$s zypper install -y gcc-ada" "apk:$s apk add gcc-gnat" "brew:brew install gnat"; do
        command -v "${try%%:*}" >/dev/null 2>&1 && { c=${try#*:}; break; }
    done
    if [ -z "$c" ]; then
        gnat_note="no GNAT, and no package manager (apt-get/dnf/pacman/zypper/apk/brew) to install it with"
        return 1
    fi
    pulse "installing GNAT to compare against"; eval "$c" >/dev/null 2>&1; pulse_stop
    command -v gnatmake >/dev/null 2>&1 && return 0
    gnat_note="tried '${c#* }' but gnatmake is still not on PATH"
    return 1
}

have_gnat(){
    [ "$NO_GNAT" = 1 ] && return 1
    command -v gnatmake >/dev/null 2>&1 || install_gnat
}

corpus_ready(){ unpack_suite "$HERE" acats; }

corpus_files(){
    local f; local -a all=()
    for f in "$HERE"/acats/*.ada; do [ -f "$f" ] && all+=("$f"); done
    [ ${#all[@]} -gt 0 ] || return 0
    printf '%s\n' "${all[@]}" | head -n "$CORPUS"
}

load_corpus(){
    CORPUS_FILES=()
    corpus_ready || return 1
    mapfile -t CORPUS_FILES < <(corpus_files)
    [ ${#CORPUS_FILES[@]} -gt 0 ]
}

monster(){
    local n=$1 i
    printf 'with TEXT_IO; use TEXT_IO;\nprocedure Monster is\n'
    printf '   package Int_IO is new Integer_IO (Integer);\n   Seed : Integer;\n   Total : Integer := 0;\n'
    for ((i = 1; i <= n; i++)); do
        printf '   type Kind_%d is (Red_%d, Green_%d, Blue_%d);\n' $i $i $i $i
        printf '   subtype Narrow_%d is Integer range %d .. %d;\n' $i $i $((i + 500))
        printf '   type Rec_%d is record\n      A : Narrow_%d;\n      B : Kind_%d;\n      C : Float;\n   end record;\n' $i $i $i
        printf '   function Fold_%d (X : Integer) return Integer;\n' $i
    done
    for ((i = 1; i <= n; i++)); do
        printf '   function Fold_%d (X : Integer) return Integer is\n      V : Integer := X;\n      R : Rec_%d;\n   begin\n' $i $i
        printf '      R.B := Green_%d;\n      R.C := Float (V mod 7);\n' $i
        printf '      case V mod 4 is\n         when 0 => V := V + %d;\n         when 1 => V := V - %d;\n' $i $i
        printf '         when 2 => V := V * 2 + ((V + %d) - (V - %d) * 1);\n         when others => V := V / 2;\n      end case;\n' $i $i
        printf '      return ((V + 1) * (V + 2) - (V + 3) + Integer (R.C)) mod 1_000_003;\n   end;\n'
    done
    printf 'begin\n   Int_IO.Get (Seed);\n'
    for ((i = 1; i <= n; i++)); do printf '   Total := (Total + Fold_%d (Seed + %d)) mod 1_000_003;\n' $i $i; done
    printf "   Put_Line (\"monster:\" & Integer'Image (Total));\nend;\n"
}

write_programs(){
    mkdir -p "$BENCH_WORK/src"; echo 1 > "$seed"

    cat > "$BENCH_WORK/src/sieve.ada" <<'EOF'
with TEXT_IO; use TEXT_IO;
procedure Sieve is
   package Int_IO is new Integer_IO (Integer);
   Limit : constant := 2_000_000;
   type Flags is array (2 .. Limit) of Boolean;
   Seed  : Integer;
   Prime : Flags;
   Count : Integer := 0;
begin
   Int_IO.Get (Seed);
   for Pass in 1 .. 5 loop
      Count := 0;
      for I in Prime'Range loop Prime (I) := True; end loop;
      for I in Prime'Range loop
         if Prime (I) then
            Count := Count + Seed;
            declare
               J : Integer := I * 2;
            begin
               while J <= Limit loop
                  Prime (J) := False;
                  J := J + I;
               end loop;
            end;
         end if;
      end loop;
   end loop;
   Put_Line ("primes:" & Integer'Image (Count));
end;
EOF

    cat > "$BENCH_WORK/src/matmul.ada" <<'EOF'
with TEXT_IO; use TEXT_IO;
procedure Matmul is
   package Int_IO is new Integer_IO (Integer);
   N : constant := 400;
   type Matrix is array (1 .. N, 1 .. N) of Float;
   Seed    : Integer;
   A, B, C : Matrix;
   Sum     : Float;
begin
   Int_IO.Get (Seed);
   for I in 1 .. N loop
      for J in 1 .. N loop
         A (I, J) := Float (I + J * Seed);
         B (I, J) := Float (I - J);
         C (I, J) := 0.0;
      end loop;
   end loop;
   for I in 1 .. N loop
      for J in 1 .. N loop
         Sum := 0.0;
         for K in 1 .. N loop
            Sum := Sum + A (I, K) * B (K, J);
         end loop;
         C (I, J) := Sum;
      end loop;
   end loop;
   Sum := 0.0;
   for I in 1 .. N loop
      for J in 1 .. N loop Sum := Sum + C (I, J); end loop;
   end loop;
   Put_Line ("checksum:" & Integer'Image (Integer (Sum / 1.0E9)));
end;
EOF

    cat > "$BENCH_WORK/src/lu.ada" <<'EOF'
with TEXT_IO; use TEXT_IO;
procedure LU is
   package Int_IO is new Integer_IO (Integer);
   N     : constant Integer := 1000;
   Last  : constant Integer := N - 1;
   type Matrix is array (1 .. N, 1 .. N) of Float;
   Seed  : Integer;
   A     : Matrix;
   Pivot : Float;
   Total : Float := 0.0;
begin
   Int_IO.Get (Seed);
   for I in 1 .. N loop
      for J in 1 .. N loop
         if I = J then A (I, J) := Float (N + I * Seed);
         else A (I, J) := Float ((I * 7 + J * 3) mod 17) - 8.0; end if;
      end loop;
   end loop;
   for K in 1 .. Last loop
      Pivot := A (K, K);
      for I in K + 1 .. N loop
         A (I, K) := A (I, K) / Pivot;
         for J in K + 1 .. N loop
            A (I, J) := A (I, J) - A (I, K) * A (K, J);
         end loop;
      end loop;
   end loop;
   for I in 1 .. N loop Total := Total + A (I, I); end loop;
   Put_Line ("lu:" & Integer'Image (Integer (Total / 100.0)));
end;
EOF

    cat > "$BENCH_WORK/src/recurse.ada" <<'EOF'
with TEXT_IO; use TEXT_IO;
procedure Recurse is
   package Int_IO is new Integer_IO (Integer);
   Seed  : Integer;
   Total : Integer := 0;
   function Fib (N : Integer) return Integer is
   begin
      if N < 2 then return N; end if;
      return Fib (N - 1) + Fib (N - 2);
   end;
begin
   Int_IO.Get (Seed);
   for I in 1 .. 6 loop
      Total := Total + Fib (30 + Seed);
   end loop;
   Put_Line ("fib:" & Integer'Image (Total));
end;
EOF

    cat > "$BENCH_WORK/src/strings.ada" <<'EOF'
with TEXT_IO; use TEXT_IO;
procedure Strings is
   package Int_IO is new Integer_IO (Integer);
   subtype Line is String (1 .. 64);
   Seed : Integer;
   Buf  : Line := (others => 'a');
   Hits : Integer := 0;
   function Count_Char (S : String; C : Character) return Integer is
      N : Integer := 0;
   begin
      for I in S'Range loop
         if S (I) = C then N := N + 1; end if;
      end loop;
      return N;
   end;
begin
   Int_IO.Get (Seed);
   for Pass in 1 .. 3_000_000 loop
      Buf (1 + ((Pass * Seed) mod 64)) := Character'Val (97 + (Pass mod 26));
      Hits := Hits + Count_Char (Buf (1 .. 32), 'a');
   end loop;
   Put_Line ("hits:" & Integer'Image (Hits));
end;
EOF

    cat > "$BENCH_WORK/src/numerics.ada" <<'EOF'
with TEXT_IO; use TEXT_IO;
procedure Numerics is
   package Int_IO is new Integer_IO (Integer);
   type Money is delta 0.01 range -1_000_000.0 .. 1_000_000.0;
   type Angle is digits 12 range -1.0E9 .. 1.0E9;
   Seed  : Integer;
   Acc   : Money := 0.0;
   Rate  : Money := 0.07;
   Theta : Angle := 0.0;
   Tally : Integer := 0;
begin
   Int_IO.Get (Seed);
   for I in 1 .. 40_000_000 loop
      Acc := Acc + Rate * Seed;
      if Acc > 900_000.0 then Acc := 0.0; end if;
      Theta := Theta + Angle (I mod 1024) * 1.0E-3;
      if Theta > 9.0E8 then Theta := 0.0; end if;
   end loop;
   Tally := Integer (Acc) / 1000 + Integer (Theta / 1.0E6);
   Put_Line ("numerics:" & Integer'Image (Tally));
end;
EOF

    cat > "$BENCH_WORK/src/checks.ada" <<'EOF'
with TEXT_IO; use TEXT_IO;
procedure Checks is
   package Int_IO is new Integer_IO (Integer);
   subtype Small is Integer range 0 .. 999;
   type Table is array (Small) of Small;
   Seed  : Integer;
   T     : Table := (others => 0);
   Idx   : Small := 0;
   Tally : Integer := 0;
begin
   Int_IO.Get (Seed);
   for Pass in 1 .. 60_000 loop
      for I in Small loop
         Idx := Small ((I * 7 + Pass * Seed) mod 1000);
         T (Idx) := Small ((T (Idx) + I + Pass + Seed) mod 997);
      end loop;
   end loop;
   for I in Small loop Tally := (Tally + T (I)) mod 1_000_003; end loop;
   Put_Line ("checks:" & Integer'Image (Tally));
end;
EOF

    cat > "$BENCH_WORK/src/exceptions.ada" <<'EOF'
with TEXT_IO; use TEXT_IO;
procedure Exceptions is
   package Int_IO is new Integer_IO (Integer);
   Trouble : exception;
   Seed    : Integer;
   Caught  : Integer := 0;
   procedure Deep (Level : Integer) is
   begin
      if Level <= 0 then raise Trouble; end if;
      Deep (Level - 1);
   end;
begin
   Int_IO.Get (Seed);
   for I in 1 .. 2_000_000 loop
      begin
         Deep (8 * Seed);
      exception
         when Trouble => Caught := Caught + 1;
      end;
   end loop;
   Put_Line ("caught:" & Integer'Image (Caught));
end;
EOF

    cat > "$BENCH_WORK/src/memory.ada" <<'EOF'
with TEXT_IO, UNCHECKED_DEALLOCATION; use TEXT_IO;
procedure Memory is
   package Int_IO is new Integer_IO (Integer);
   type Node;
   type Link is access Node;
   type Node is record
      Value : Integer;
      Next  : Link;
   end record;
   procedure Free is new Unchecked_Deallocation (Node, Link);
   Seed  : Integer;
   Head  : Link;
   N     : Link;
   Tally : Integer := 0;
begin
   Int_IO.Get (Seed);
   for Pass in 1 .. 3_000 loop
      Head := null;
      for I in 1 .. 5_000 loop
         N := new Node'(Value => I * Seed, Next => Head);
         Head := N;
      end loop;
      while Head /= null loop
         Tally := (Tally + Head.Value) mod 1_000_003;
         N := Head;
         Head := Head.Next;
         Free (N);
      end loop;
   end loop;
   Put_Line ("memory:" & Integer'Image (Tally));
end;
EOF

    cat > "$BENCH_WORK/src/tasking.ada" <<'EOF'
with TEXT_IO; use TEXT_IO;
procedure Tasking is
   package Int_IO is new Integer_IO (Integer);
   Seed  : Integer;
   Total : Integer := 0;
   task Server is
      entry Push (V : Integer);
      entry Drain (V : out Integer);
   end Server;
   task body Server is
      Acc : Integer := 0;
   begin
      loop
         select
            accept Push (V : Integer) do Acc := Acc + V; end Push;
         or
            accept Drain (V : out Integer) do V := Acc; end Drain;
            exit;
         end select;
      end loop;
   end;
begin
   Int_IO.Get (Seed);
   for I in 1 .. 200_000 loop
      Server.Push (Seed);
   end loop;
   Server.Drain (Total);
   Put_Line ("rendezvous:" & Integer'Image (Total));
end;
EOF

    cat > "$BENCH_WORK/src/taskflood.ada" <<'EOF'
with TEXT_IO; use TEXT_IO;
procedure Taskflood is
   package Int_IO is new Integer_IO (Integer);
   Seed  : Integer;
   Total : Integer := 0;
   task type Worker is
      entry Take (V : Integer);
      entry Give (V : out Integer);
   end Worker;
   task body Worker is
      Mine : Integer := 0;
   begin
      accept Take (V : Integer) do Mine := V; end Take;
      for I in 1 .. 50 loop Mine := Mine + I; end loop;
      accept Give (V : out Integer) do V := Mine; end Give;
   end;
begin
   Int_IO.Get (Seed);
   for Round in 1 .. 400 loop
      declare
         Crew : array (1 .. 8) of Worker;
         Got  : Integer;
      begin
         for W in Crew'Range loop Crew (W).Take (W * Seed); end loop;
         for W in Crew'Range loop
            Crew (W).Give (Got);
            Total := (Total + Got) mod 1_000_003;
         end loop;
      end;
   end loop;
   Put_Line ("flood:" & Integer'Image (Total));
end;
EOF

    cat > "$BENCH_WORK/src/taskselect.ada" <<'EOF'
with TEXT_IO; use TEXT_IO;
procedure Taskselect is
   package Int_IO is new Integer_IO (Integer);
   Seed  : Integer;
   Total : Integer := 0;
   task Arbiter is
      entry Left  (V : Integer);
      entry Right (V : Integer);
      entry Done  (V : out Integer);
   end Arbiter;
   task body Arbiter is
      Acc : Integer := 0;
   begin
      loop
         select
            accept Left (V : Integer) do Acc := Acc + V; end Left;
         or
            accept Right (V : Integer) do Acc := Acc - V; end Right;
         or
            accept Done (V : out Integer) do V := Acc; end Done;
            exit;
         else
            Acc := Acc + 1;
         end select;
      end loop;
   end;
begin
   Int_IO.Get (Seed);
   for I in 1 .. 100_000 loop
      if I mod 2 = 0 then Arbiter.Left (Seed); else Arbiter.Right (Seed); end if;
   end loop;
   Arbiter.Done (Total);
   Put_Line ("select: done");
end;
EOF

    cat > "$BENCH_WORK/src/taskelse.ada" <<'EOF'
with TEXT_IO; use TEXT_IO;
procedure Taskelse is
   package Int_IO is new Integer_IO (Integer);
   Seed  : Integer;
   Total : Integer := 0;
   task Poller is
      entry Never (V : Integer);
      entry Done  (V : out Integer);
   end Poller;
   task body Poller is
      Acc : Integer := 0;
   begin
      for I in 1 .. 2_000_000 loop
         select
            accept Never (V : Integer) do Acc := Acc + V; end Never;
         else
            Acc := Acc + 1;
         end select;
      end loop;
      accept Done (V : out Integer) do V := Acc; end Done;
   end;
begin
   Int_IO.Get (Seed);
   Poller.Done (Total);
   Put_Line ("else:" & Integer'Image (Total));
end;
EOF

    cat > "$BENCH_WORK/src/indirect.ada" <<'EOF'
with TEXT_IO; use TEXT_IO;
procedure Indirect is
   package Int_IO is new Integer_IO (Integer);
   type Op is access function (X : Integer) return Integer;
   function Add_One (X : Integer) return Integer;
   function Double  (X : Integer) return Integer;
   function Negate  (X : Integer) return Integer;
   function Square  (X : Integer) return Integer;
   Table : constant array (0 .. 3) of Op :=
     (Add_One'Access, Double'Access, Negate'Access, Square'Access);
   Seed  : Integer;
   Total : Integer := 0;
   function Add_One (X : Integer) return Integer is begin return X + 1; end;
   function Double  (X : Integer) return Integer is begin return X + X; end;
   function Negate  (X : Integer) return Integer is begin return -X; end;
   function Square  (X : Integer) return Integer is begin return X * X; end;
begin
   Int_IO.Get (Seed);
   for I in 1 .. 30_000_000 loop
      Total := (Total + Table (I mod 4) (I mod 512)) mod 1024;
   end loop;
   Put_Line ("indirect:" & Integer'Image (Total));
end;
EOF


    cat > "$BENCH_WORK/src/monitor.ada" <<'EOF'
with TEXT_IO; use TEXT_IO;
procedure Monitor is
   package Int_IO is new Integer_IO (Integer);
   protected Cell is
      procedure Put (X : Integer);
      function  Get return Integer;
      entry     Take (X : out Integer);
   private
      V : Integer := 0;
   end Cell;
   Seed  : Integer;
   Total : Integer := 0;
   Held  : Integer;
   protected body Cell is
      procedure Put (X : Integer) is begin V := (V + X) mod 1024; end Put;
      function  Get return Integer is begin return V; end Get;
      entry     Take (X : out Integer) when True is begin X := V; end Take;
   end Cell;
begin
   Int_IO.Get (Seed);
   for I in 1 .. 4_000_000 loop
      Cell.Put (I mod 512);
      Total := (Total + Cell.Get) mod 1024;
      Cell.Take (Held);
      Total := (Total + Held) mod 1024;
   end loop;
   Put_Line ("monitor:" & Integer'Image (Total));
end;
EOF


    cat > "$BENCH_WORK/src/finalizer.ada" <<'EOF'
with ADA.FINALIZATION; use ADA.FINALIZATION;
with TEXT_IO; use TEXT_IO;
procedure Finalizer is
   package Int_IO is new Integer_IO (Integer);
   Events : Integer := 0;
   package Items is
      type Item is new Controlled with record V : Integer := 0; end record;
      procedure Initialize (I : in out Item);
      procedure Adjust     (I : in out Item);
      procedure Finalize   (I : in out Item);
   end Items;
   use Items;
   Seed : Integer;
   package body Items is
      procedure Initialize (I : in out Item) is begin I.V := Events mod 7; Events := Events + I.V + 1; end Initialize;
      procedure Adjust     (I : in out Item) is begin I.V := (I.V + Events) mod 13; Events := Events + I.V + 3; end Adjust;
      procedure Finalize   (I : in out Item) is begin Events := Events + I.V + 5; end Finalize;
   end Items;
begin
   Int_IO.Get (Seed);
   declare
      Keep : Item;
   begin
      for I in 1 .. 2_000_000 loop
         declare
            Fresh : Item;
         begin
            Fresh.V := I mod 512;
            Keep := Fresh;
         end;
      end loop;
      Events := Events + Keep.V;
   end;
   Put_Line ("finalize:" & Integer'Image (Events));
end;
EOF


    cat > "$BENCH_WORK/src/wraparound.ada" <<'EOF'
with TEXT_IO; use TEXT_IO;
procedure Wraparound is
   package Int_IO is new Integer_IO (Integer);
   type Byte  is mod 256;
   type Odd   is mod 200;
   Seed  : Integer;
   B     : Byte    := 0;
   D     : Odd     := 0;
   Total : Integer := 0;
begin
   Int_IO.Get (Seed);
   for I in 1 .. 10_000_000 loop
      B := B + Byte (I mod 251);
      B := (B * 3) xor 16#5A#;
      D := D * 7 + Odd (I mod 199);
      Total := (Total + Integer (B) + Integer (D)) mod 1024;
   end loop;
   Put_Line ("wraparound:" & Integer'Image (Total));
end;
EOF


    local p
    for p in $ALL_PROGRAMS; do cp "$BENCH_WORK/src/$p.ada" "$BENCH_WORK/src/$p.adb"; done
}

run_stages(){
    local p f w n=0 total=${#PROGRAM_LIST[@]}
    heading "COMPILE STAGES"
    printf '  %-11s %11s %11s %11s %9s\n' program "front end" whole "back end" "front %"; rule
    for p in "${PROGRAM_LIST[@]}"; do
        progress $((n++)) "$total" "staging $p"
        f=$(med "$(measure "$BENCH_ADA83" --ir "$BENCH_WORK/src/$p.ada" -o "$BENCH_WORK/$p.ll")")
        w=$(med "$(measure "$BENCH_ADA83" "-O$OPT" "$BENCH_WORK/src/$p.ada" -o "$BENCH_WORK/$p.exe")")
        clear_line
        awk -v p="$p" -v f="$f" -v w="$w" 'BEGIN{ b=w-f
            printf "  %-11s %11s %11s %11.3f %8.0f%%\n", p, f, w, (b>0?b:0), (w>0?f*100/w:0) }'
    done
    clear_line
}

run_parser(){
    local n t lines first='' firstlines=''
    heading "FRONT END AGAINST INPUT SIZE"
    printf '  %-8s %9s %11s %13s %s\n' units lines "front end" "µs per line" ""; rule
    for n in 50 100 200 400 800; do
        progress "$n" 800 "generating and compiling ${n} units"
        monster "$n" > "$BENCH_WORK/src/monster.ada"
        lines=$(wc -l < "$BENCH_WORK/src/monster.ada")
        t=$(med "$(measure "$BENCH_ADA83" --ir "$BENCH_WORK/src/monster.ada" -o "$BENCH_WORK/monster.ll")")
        clear_line
        [ -z "$first" ] && { first=$t; firstlines=$lines; }
        awk -v n="$n" -v l="$lines" -v t="$t" -v f="$first" -v fl="$firstlines" 'BEGIN{
            per = t*1000000/l
            base = f*1000000/fl
            printf "  %-8s %9s %11s %13.1f %s\n", n, l, t, per,
                   (base>0 ? sprintf("%.2fx the cost per line of the smallest", per/base) : "") }'
    done
    clear_line
    printf '\n  %sCost per line should stay flat. A figure that climbs with size is a\n' "$DIM"
    printf '  super-linear algorithm in the front end, and worth hunting down.%s\n' "$OFF"
}

run_corpus(){
    heading "CORPUS THROUGHPUT"
    load_corpus || { echo "  no corpus available"; return; }
    local n lines secs f finished=0 one peak
    n=${#CORPUS_FILES[@]}
    lines=$(cat "${CORPUS_FILES[@]}" 2>/dev/null | wc -l); : > "$BENCH_WORK/times"
    TIMEFORMAT=%R
    secs=$( { time { for f in "${CORPUS_FILES[@]}"; do
            one=$( { time "$BENCH_ADA83" --ir "$f" -o "$BENCH_WORK/c.ll" >/dev/null 2>&1; } 2>&1 )
            printf '%s\t%s\n' "$one" "$(basename "$f")" >> "$BENCH_WORK/times"
            finished=$((finished+1)); [ $((finished % 10)) = 0 ] && progress "$finished" "$n" "compiling the suite"
         done ; } ; } 2>&1 | tail -1 )
    clear_line
    printf '  %-22s %s\n' files "$n"
    printf '  %-22s %s\n' lines "$lines"
    printf '  %-22s %s s\n' "wall time" "$secs"
    awk -v l="$lines" -v s="$secs" 'BEGIN{if(s+0>0)printf "  %-22s %d\n","lines per second",l/s}'
    awk -v c="$n" -v s="$secs" 'BEGIN{if(s+0>0)printf "  %-22s %.1f\n","files per second",c/s}'
    heading "SLOWEST INPUTS"
    peak=$(sort -rn "$BENCH_WORK/times" | head -1 | cut -f1)
    sort -rn "$BENCH_WORK/times" | head -n "$SLOWEST" | while IFS=$'\t' read -r t name; do
        printf '  %-18s %7ss  %s\n' "$name" "$t" "$(bar "$(scaled "$t" "$peak" 24)" 24)"
    done
}

run_compare(){
    local other=$1 p a b sa sb n=0 total=${#PROGRAM_LIST[@]} f ref new
    [ -n "$other" ] || die "compare needs the path of another ada83 binary"
    [ -x "$other" ] || die "cannot run $other"
    heading "AGAINST $(basename "$other")"
    printf '  %-11s %14s %14s %10s   %s\n' program reference "this build" delta ''; rule
    for p in "${PROGRAM_LIST[@]}"; do
        progress $((n++)) "$total" "compiling $p"
        sb=$(measure "$other" "-O$OPT" "$BENCH_WORK/src/$p.ada" -o "$BENCH_WORK/$p.ref")
        sa=$(measure "$BENCH_ADA83" "-O$OPT" "$BENCH_WORK/src/$p.ada" -o "$BENCH_WORK/$p.new")
        b=$(med "$sb"); a=$(med "$sa"); clear_line
        printf '  %-11s %8s%s %8s%s %s   %s\n' "$p" \
            "$b" "$(spread "$(rsd "$sb")")" "$a" "$(spread "$(rsd "$sa")")" \
            "$(change "$a" "$b")" "$(verdict "$a" "$b" "$(rsd "$sa")" "$(rsd "$sb")")"
    done
    clear_line
    load_corpus || return 0
    TIMEFORMAT=%R
    progress 1 2 "reference over the corpus"
    ref=$( { time { for f in "${CORPUS_FILES[@]}"; do "$other" --ir "$f" -o "$BENCH_WORK/c.ll" >/dev/null 2>&1; done ; } ; } 2>&1 | tail -1 )
    progress 2 2 "this build over the corpus"
    new=$( { time { for f in "${CORPUS_FILES[@]}"; do "$BENCH_ADA83" --ir "$f" -o "$BENCH_WORK/c.ll" >/dev/null 2>&1; done ; } ; } 2>&1 | tail -1 )
    clear_line; rule
    printf '  %-11s %14s %14s %s\n' corpus "$ref" "$new" "$(change "$new" "$ref")"
}

run_profile(){
    heading "PROFILE"
    load_corpus || { echo "  no corpus available"; return; }
    local sweep=$BENCH_WORK/sweep.sh
    cat > "$sweep" <<EOF
#!/bin/sh
for f do "$BENCH_ADA83" --ir "\$f" -o "$BENCH_WORK/c.ll" >/dev/null 2>&1; done
EOF
    chmod +x "$sweep"
    if command -v perf >/dev/null 2>&1 && perf stat true >/dev/null 2>&1; then
        pulse "recording with perf"
        perf record -q -o "$BENCH_WORK/perf.data" -- "$sweep" "${CORPUS_FILES[@]}" >/dev/null 2>&1
        pulse_stop
        perf report -i "$BENCH_WORK/perf.data" --stdio --no-children -F overhead,symbol 2>/dev/null \
            | grep -E '^\s+[0-9]' | head -20 | sed 's/^/  /'
        return
    fi
    pulse "building an instrumented compiler"
    gcc -O2 -pg -w -std=gnu2x -o "$BENCH_WORK/ada83-pg" "$HERE/ada83.c" -lpthread >/dev/null 2>&1
    pulse_stop
    [ -x "$BENCH_WORK/ada83-pg" ] || { echo "  perf is absent and the instrumented build failed"; return; }
    pulse "profiling"
    ( cd "$BENCH_WORK" && for f in "${CORPUS_FILES[@]}"; do "$BENCH_WORK/ada83-pg" --ir "$f" -o "$BENCH_WORK/c.ll" >/dev/null 2>&1; done )
    pulse_stop
    if [ -f "$BENCH_WORK/gmon.out" ] && command -v gprof >/dev/null 2>&1; then
        ( cd "$BENCH_WORK" && gprof -b -p "$BENCH_WORK/ada83-pg" gmon.out 2>/dev/null | head -22 | sed 's/^/  /' )
    else echo "  no profile was produced"; fi
}

run_codegen(){
    local gnat=$1 suite=${2:-1} p a g am gm na ng oa og n=0 total=${#PROGRAM_LIST[@]} peak=0
    local outcome r note tsv='' cpus=$BENCH_CPU lpk=0 v
    local -a DIALECT
    LOAD_PEAK=0
    if [ -n "$TSV_DIR" ]; then mkdir -p "$TSV_DIR"; tsv=$TSV_DIR/suite$suite.tsv
        printf 'program\tada83_med\tada83_mad\tgnat_med\tgnat_mad\tdistinguishable\tratio\toutput\tsamples\tcpus\tload_peak\n' > "$tsv"
    fi
    if [ "$SUITES" -gt 1 ]
    then heading "GENERATED CODE — suite $suite of $SUITES" "median ± MAD of $CODEGEN_REPEATS interleaved repetitions, $PIN_WHY"
    else heading "GENERATED CODE" "median ± MAD of $CODEGEN_REPEATS interleaved repetitions, $PIN_WHY"; fi
    if [ "$gnat" = 1 ]
    then printf '  %-11s %15s %15s %7s  %-17s %s\n' program 'ada83 (s)' 'gnat (s)' ratio verdict stresses
    else printf '  %-11s %15s  %s\n' program 'ada83 (s)' stresses; fi
    rule
    for p in "${PROGRAM_LIST[@]}"; do
        progress $((n++)) "$total" "$p"
        if ! "$BENCH_ADA83" "-O$OPT" \
               "$BENCH_WORK/src/$p.ada" -o "$BENCH_WORK/$p.a83" >/dev/null 2>&1; then
            clear_line; printf '  %-11s %14s\n' "$p" "build failed"
            eval "T_$p=x G_$p=x R_${suite}_$p=- D_${suite}_$p=0"; continue
        fi
        g=x gm=x ng=0 og=''
        if concurrent "$p"
        then PIN=(${PIN_MANY[@]+"${PIN_MANY[@]}"}); cpus=$BENCH_CPUS_TASK
        else PIN=(${PIN_ONE[@]+"${PIN_ONE[@]}"});  cpus=$BENCH_CPU; fi
        load_watch_start
        if [ "$gnat" = 1 ] &&
           ( cd "$BENCH_WORK/src" && gnatmake -q "-O$OPT" "$p.adb" -o "$BENCH_WORK/$p.gnat" ) >/dev/null 2>&1
        then measure_pair "$BENCH_WORK/$p.a83" "$BENCH_WORK/$p.gnat"
             read -r g gm ng <<<"$(stats "$BENCH_WORK/samples.b")"
        else measure_pair "$BENCH_WORK/$p.a83" ''; fi
        load_watch_stop; lpk=$LOAD_LAST
        read -r a am na <<<"$(stats "$BENCH_WORK/samples.a")"
        oa=$("$BENCH_WORK/$p.a83" <"$seed" 2>&1 | head -1)
        [ "$g" = x ] || og=$("$BENCH_WORK/$p.gnat" <"$seed" 2>&1 | head -1)
        eval "T_$p=\$a G_$p=\$g AM_$p=\$am GM_$p=\$gm"
        clear_line
        if [ "$gnat" = 1 ]; then
            note=same
            if [ "$g" != x ] && [ "$oa" != "$og" ]
            then comparable "$p" && note=DIFFERS || note=representation; fi
            if distinguishable "$a" "$am" "$g" "$gm"; then
                r=$(awk -v a="$a" -v b="$g" 'BEGIN{printf "%.2f", a/b}')
                outcome=$(speedup "$a" "$g")
                case $outcome in *faster) outcome="$GOOD$outcome$OFF" ;; *slower) outcome="$BAD$outcome$OFF" ;; esac
                eval "R_${suite}_$p=\$r D_${suite}_$p=1"
            else
                r='-'; outcome="${DIM}indistinguishable$OFF"
                eval "R_${suite}_$p=- D_${suite}_$p=0"
            fi
            eval "MA_${suite}_$p=\$a MG_${suite}_$p=\$g"
            printf '  %-11s %s %s %7s  %-17s %s' "$p" "$(pm "$a" "$am")" "$(pm "$g" "$gm")" \
                "$r" "$outcome" "$(describe "$p")"
            case $note in DIFFERS) printf '  %sOUTPUT DIFFERS%s' "$BAD" "$OFF" ;;
                          representation) printf '  %s(totals differ — the standard lets a fixed point type pick its own small)%s' \
                              "$DIM" "$OFF" ;; esac
            awk -v l="$lpk" -v m="$LOAD_MAX" 'BEGIN{exit !(l+0 > m+0)}' &&
                printf '  %s! load reached %s while measuring%s' "$BAD" "$lpk" "$OFF"
            printf '\n'
            [ -n "$tsv" ] && printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s/%s\t%s\t%s\n' \
                "$p" "$a" "$am" "$g" "$gm" \
                "$(eval "printf %s \$D_${suite}_$p")" "$r" "$note" "$na" "$ng" "$cpus" "$lpk" >> "$tsv"
        else
            printf '  %-11s %s  %s\n' "$p" "$(pm "$a" "$am")" "$(describe "$p")"
            [ -n "$tsv" ] && printf '%s\t%s\t%s\t-\t-\t-\t-\t-\t%s/0\t%s\t%s\n' "$p" "$a" "$am" "$na" "$cpus" "$lpk" >> "$tsv"
        fi
    done
    clear_line
    printf '\n  %sload%s    %s at the end of suite %s, %s at its worst while measuring%s\n' \
        "$DIM" "$OFF" "$(loadavg)" "$suite" "$LOAD_PEAK" \
        "$(awk -v l="$LOAD_PEAK" -v m="$LOAD_MAX" 'BEGIN{print (l+0>m+0)?" — SOMETHING ELSE WAS RUNNING, TREAT THIS SUITE AS SUSPECT":""}')"
    [ "$gnat" = 1 ] || return 0
    heading "SIDE BY SIDE"
    for p in "${PROGRAM_LIST[@]}"; do
        eval "a=\${T_$p:-x}"; eval "g=\${G_$p:-x}"
        for v in "$a" "$g"; do
            case $v in x) ;; *) awk -v v="$v" -v m="$peak" 'BEGIN{exit !(v>m)}' && peak=$v ;; esac
        done
    done
    for p in "${PROGRAM_LIST[@]}"; do
        eval "a=\${T_$p:-x}"; eval "g=\${G_$p:-x}"; eval "n=\${D_${suite}_$p:-0}"
        [ "$a" = x ] && continue
        outcome=$(speedup "$a" "$g"); [ "$n" = 1 ] || outcome='indistinguishable'
        printf '  %-11s %s%-5s%s %s %7s  %s\n' "$p" "$BOLD" ada83 "$OFF" \
            "$(bar "$(scaled "$a" "$peak" 30)" 30)" "$a" "$outcome"
        [ "$g" = x ] && continue
        printf '  %-11s %s%-5s%s %s %7s\n' '' "$DIM" gnat "$OFF" \
            "$(bar "$(scaled "$g" "$peak" 30)" 30)" "$g"
    done
    return 0
}

run_codegen_suites(){
    local gnat=$1 s
    load_gate
    for ((s = 1; s <= SUITES; s++)); do run_codegen "$gnat" "$s"; done
    [ "$SUITES" -gt 1 ] && [ "$gnat" = 1 ] && run_stability
    return 0
}

run_stability(){
    local p r1 r2 d1 d2 a1 g1 a2 g2
    heading "SUITE AGAINST SUITE" "the same ratio measured twice, as the check that it is real"
    printf '  %-11s %9s %9s %10s  %s\n' program 'suite 1' 'suite 2' drift ''; rule
    for p in "${PROGRAM_LIST[@]}"; do
        eval "r1=\${R_1_$p:-x} r2=\${R_2_$p:-x} d1=\${D_1_$p:-0} d2=\${D_2_$p:-0}"
        eval "a1=\${MA_1_$p:-x} g1=\${MG_1_$p:-x} a2=\${MA_2_$p:-x} g2=\${MG_2_$p:-x}"
        [ "$r1" = x ] && continue
        if [ "$d1" != 1 ] || [ "$d2" != 1 ]; then
            printf '  %-11s %9s %9s %10s  %s\n' "$p" "$r1" "$r2" '-' \
                "$([ "$d1" = "$d2" ] && echo 'indistinguishable in both suites' \
                   || echo 'DISTINGUISHABLE IN ONE SUITE ONLY — not published')"
            continue
        fi
        awk -v p="$p" -v a1="$a1" -v g1="$g1" -v a2="$a2" -v g2="$g2" 'BEGIN{
            r1 = a1 / g1; r2 = a2 / g2; d = (r2 - r1) * 100 / r1
            printf "  %-11s %9.3f %9.3f %+9.1f%%  %s\n", p, r1, r2, d,
                ((d < 0 ? -d : d) <= 10) ? "stable" \
                                         : "UNSTABLE — NO RATIO SHOULD BE PUBLISHED FOR THIS ROW" }'
    done
}

run_memory(){
    local p c r n=0 total=${#PROGRAM_LIST[@]}
    heading "PEAK MEMORY" "megabytes"
    printf '  %-11s %14s %14s  %s\n' program "compiling it" "running it" stresses; rule
    for p in "${PROGRAM_LIST[@]}"; do
        progress $((n++)) "$total" "$p"
        c=$(peak_rss "$BENCH_ADA83" "-O$OPT" "$BENCH_WORK/src/$p.ada" -o "$BENCH_WORK/$p.mem")
        r=x; [ -x "$BENCH_WORK/$p.mem" ] && r=$(peak_rss "$BENCH_WORK/$p.mem")
        clear_line
        printf '  %-11s %14s %14s  %s\n' "$p" "${c:-x}" "${r:-x}" "$(describe "$p")"
    done
    clear_line
}

cpu_model(){
    if [ -r /proc/cpuinfo ]; then sed -n 's/^model name[[:space:]]*: //p' /proc/cpuinfo | head -1
    else sysctl -n machdep.cpu.brand_string 2>/dev/null; fi
}

build_flags(){
    [ -n "${ADA83_BUILD_FLAGS:-}" ] && { printf '%s\n' "$ADA83_BUILD_FLAGS"; return; }
    make -C "$HERE" -s --eval="bench-print-flags: ; @echo \$(CC) \$(CFLAGS) \$(WHOLE_PROGRAM) \$(TUNE)" \
        bench-print-flags 2>/dev/null | head -1
}

bench_header(){
    local mode=$1 gnat=$2
    printf '\n  %sada83%s   %s\n' "$BOLD" "$OFF" "$("$BENCH_ADA83" --version 2>&1 | head -1)"
    printf '  %sbuilt%s   %s\n' "$BOLD" "$OFF" "$(build_flags)"
    if [ "$gnat" = 1 ]; then
        printf '  %sgnat%s    %s (%s)\n' "$BOLD" "$OFF" "$(gnatmake --version 2>&1 | head -1)" \
            "$(gcc --version 2>&1 | head -1)"
    elif [ -n "${gnat_note:-}" ]; then
        printf '  %sgnat%s    %s\n' "$BOLD" "$OFF" "$gnat_note"
    fi
    printf '  %shost%s    %s %s, %s cpus' "$BOLD" "$OFF" "$(uname -s)" "$(uname -m)" \
        "$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo '?')"
    [ -n "$(cpu_model)" ] && printf ', %s' "$(cpu_model)"
    printf '\n  %sload%s    %s at start (1 minute average, %s cpus)\n' "$BOLD" "$OFF" \
        "$(loadavg)" "$(nproc 2>/dev/null || echo '?')"
    case $mode in
      codegen|all) printf '  %smethod%s  benchmark programs at -O%s by both compilers; %s;\n' \
                       "$BOLD" "$OFF" "$OPT" "$PIN_WHY"
                   printf '          %s interleaved repetitions per program after %s warmup runs,\n' \
                       "$CODEGEN_REPEATS" "$CODEGEN_WARMUP"
                   printf '          reported as median ± median absolute deviation over %s suites;\n' "$SUITES"
                   printf '          a ratio is printed only where the medians differ by more than\n'
                   printf '          the sum of the two MADs (MAD floored at %ss)\n' "$FLOOR" ;;
      *)           printf '  %smethod%s  median of %s runs after %s warmup, at -O%s\n' \
                       "$BOLD" "$OFF" "$REPEATS" "$WARMUP" "$OPT" ;;
    esac
}

bench_main(){
    set +e
    case "${1:-}" in help|-h|--help) usage; exit 0 ;; esac

    local mode=${1:-all} reference=${2:-} gnat=0
    REPEATS=${REPEATS:-7} WARMUP=${WARMUP:-1} OPT=${OPT:-2}
    CORPUS=${CORPUS:-300} ONLY=${ONLY:-}
    NO_GNAT=${NO_GNAT:-0}
    CODEGEN_REPEATS=${CODEGEN_REPEATS:-25} CODEGEN_WARMUP=${CODEGEN_WARMUP:-3}
    SUITES=${SUITES:-2} RT=${RT:-0} FLOOR=${FLOOR:-0.002}
    LOAD_MAX=${LOAD_MAX:-2.0} FORCE=${FORCE:-0} NO_PIN=${NO_PIN:-0} TSV_DIR=${TSV_DIR:-}

    BENCH_ADA83=${ADA83:-$HERE/bin-$HOST_TARGET/ada83}
    [ -x "$BENCH_ADA83" ] || [ ! -x "$BENCH_ADA83.exe" ] || BENCH_ADA83=$BENCH_ADA83.exe
    [ -x "$BENCH_ADA83" ] || [ ! -x "$HERE/ada83" ] || BENCH_ADA83=$HERE/ada83

    BENCH_WORK=$(mktemp -d "${TMPDIR:-/tmp}/ada83-bench-XXXXXX")
    seed=$BENCH_WORK/seed
    LOAD_SAMPLES=$BENCH_WORK/load.samples
    PROGRAM_LIST=(); read -r -a PROGRAM_LIST <<<"${ONLY:-$ALL_PROGRAMS}"

    bench_pinning
    hide_cursor
    [ -x "$BENCH_ADA83" ] || { pulse "building the compiler"; make -C "$HERE" -s ada83 >/dev/null 2>&1; pulse_stop; }
    [ -x "$BENCH_ADA83" ] || die "cannot build $BENCH_ADA83"
    write_programs

    case $mode in codegen|all) have_gnat && gnat=1 ;; esac
    bench_header "$mode" "$gnat"

    case $mode in
        stages)  run_stages ;;
        parser)  run_parser ;;
        corpus)  run_corpus ;;
        compare) run_compare "$reference" ;;
        profile) run_profile ;;
        codegen) run_codegen_suites "$gnat" ;;
        memory)  run_memory ;;
        all)     run_stages; run_parser; run_corpus; run_codegen_suites "$gnat"; run_memory ;;
        *)       show_cursor; echo "$SELF: unknown mode '$mode'" >&2; usage >&2; exit 2 ;;
    esac
    show_cursor
    echo
}

usage(){ cat <<TEXT
Usage: $SELF [COMMAND] [ARGUMENT]

Test and measure the ada83 compiler: the ACATS conformance suite, the
extension tests, the project-file tests, the acats-bonus and debugging
suites, the reproducers, and the benchmarks.  With no arguments, runs
every test.

Commands:
  run [SELECTOR]     run the suite and print a report (the default)
  check [SELECTOR]   run, then diff against the baseline; exit 1 on regression
  bless [SELECTOR]   run, then write the results as the new baseline
  list [SELECTOR]    list the tests a selector expands to
  extensions         run only the extension tests
  project            run only the project-file tests (gpr and gpj)
  debug              run only the debugging feature tests (-g, gdb, --dump-*);
                     cases whose feature has not merged yet count as pending
  bonus              run only the acats-bonus suite: ACATS 4.2 tests for the
                     post-Ada-83 features behind -x, sanitized to Ada 83 plus
                     the feature under test; a test whose feature is still
                     pending counts as pending, never as a failure
  repro              run only the reproducers: the program each fix was landed
                     with, under repro/, run in isolation and judged by the
                     expectation lines in its header (see repro/run.sh)
  bench [MODE]       measure rather than test; see Benchmark modes below
  help               display this help and exit

A full run (no selector, or \`all') ends with the extension tests: the Ada
programs under extensions/, which cover what ACATS cannot see -- the _ada_
symbol prefix on library subprograms, and the Command_Line vendor package.
They are counted separately, as X= and XF= in the run summary.

Selectors:
  all                every test (default)
  a, b, c, d, e, l   one ACATS class
  PREFIX             a filename prefix, such as c45 or c45347

Benchmark modes, reached as \`$SELF bench MODE' or \`bash test-bench.sh MODE':
  stages              where compile time goes: front end against back end
  parser              front end against input size, to expose non-linear cost
  corpus              throughput over the conformance suite, slowest inputs named
  compare REFERENCE   this compiler against another build of it, with deltas
  profile             the functions in ada83.c that compiling spends time in
  codegen             run time of the generated code, against GNAT where present
  memory              peak memory of the compiler and of what it produces
  all                 every mode but compare and profile (the default)

To compare two builds, keep the old binary and name it:

  cp bin-*/ada83 /tmp/before && make && ./$SELF bench compare /tmp/before

Environment:
  ADA83              compiler to test (default: bin-<platform>/ada83, built
                     with make if it is missing)
  OPT                the flag the tests link with (default: -O2); under bench,
                     the level alone (default: 2)
  JOBS, NPROC        parallel workers (default: the processor count)
  TEST_TIMEOUT       per-test run cap in seconds (default: 30)
  COMPILE_TIMEOUT    per-unit compile/bind cap in seconds (default: 30)
  LINK_TIMEOUT       per-test link cap in seconds (default: 20)
  STARTUP_TIMEOUT    compiler startup/version cap in seconds (default: 30)
  BUILD_TIMEOUT      compiler rebuild cap in seconds (default: 300)
  WORKER_TIMEOUT     last-resort cap around one complete test worker
  KILL_GRACE         seconds after TERM before timeout sends KILL (default: 2)
  BASELINE           baseline manifest path (default: acats.baseline)
  SLOWEST            slowest tests or inputs to name (default: 12)
  KEEP_WORK          set to 1 to keep the working trees
  TAP                set to 1 to also write a TAP stream
  NO_ANIMATE         set to 1 to draw no progress indicators
  NO_COLOUR          set to 1 to draw no colour

Benchmark environment:
  REPEATS            timed repetitions after warmup (default: 7)
  WARMUP             untimed runs before measuring (default: 1)
  CORPUS             files to take from the conformance suite (default: 300)
  ONLY               run only the named programs, space separated
  NO_GNAT            set to 1 to skip GNAT

codegen mode only, where the two compilers are compared directly:
  CODEGEN_REPEATS    timed repetitions of each pair (default: 25)
  CODEGEN_WARMUP     untimed runs of each binary first (default: 3)
  SUITES             whole passes of the suite, to show the ratios hold (default: 2)
  BENCH_CPU          core to pin both binaries to (default: the last one)
  BENCH_CPUS_TASK    cores for the programs with tasks (default: the last two)
  RT                 set to 1 to run at SCHED_FIFO priority 50 as well
  FLOOR              seconds below which a MAD is treated as timer noise (default: 0.002)
  LOAD_MAX           one-minute load average above which the run refuses (default: 2.0)
  FORCE              set to 1 to measure anyway on a busy machine
  TSV_DIR            directory to write suite<N>.tsv into, one row per program
  NO_PIN             set to 1 to measure without taskset, nice or chrt

Each run writes results to test_results/ID/ and logs to acats_logs/ID/, where
ID is unique to the run, so concurrent runs do not overwrite one another.
TEXT
}

main(){
    if [[ ${1:-} == bench ]]; then shift; bench_main "$@"; return; fi

    local cmd=${1:-run}; shift || true
    case $cmd in
        h|help|-h|--help) usage; return ;;
    esac

    acats_setup
    case $cmd in
        run|g)   run_selector "${1:-all}" "ACATS RUN — ${1:-all}"
                 if [[ ${1:-all} == all ]]; then
                     run_extension_tests
                     run_project_tests
                     run_bonus_tests
                     run_debug_tests
                     run_repro_tests
                 fi ;;
        q)       run_selector "${1:-c32}" "ACATS RUN — ${1:-c32}" ;;
        check)   run_selector "${1:-all}" "ACATS CHECK — ${1:-all}"
                 [[ ${1:-all} == all ]] && { run_extension_tests; run_project_tests; run_bonus_tests; run_debug_tests; run_repro_tests; }
                 compare_to_baseline
                 ((REGRESSIONS==0 && ${EXT_FAIL:-0}==0 && ${PROJ_FAIL:-0}==0 && ${DBG_FAIL:-0}==0 && ${REPRO_FAIL:-0}==0)) || exit 1 ;;
        extensions|x)
                 run_extension_tests
                 ((${EXT_FAIL:-0}==0)) || exit 1 ;;
        project|p)
                 run_project_tests
                 ((${PROJ_FAIL:-0}==0)) || exit 1 ;;
        bonus|b83)
                 run_bonus_tests
                 ((${BONUS_FAIL:-0}==0)) || exit 1 ;;
        debug|dbg)
                 run_debug_tests
                 ((${DBG_FAIL:-0}==0)) || exit 1 ;;
        repro|rp)
                 run_repro_tests
                 ((${REPRO_FAIL:-0}==0)) || exit 1 ;;
        bless)   run_selector "${1:-all}" "ACATS BLESS — ${1:-all}"
                 write_baseline ;;
        list)    local -a SELECTED=(); selector_files "${1:-all}"
                 local f; for f in "${SELECTED[@]}"; do basename "$f" .ada; done ;;
        *)       usage; exit 2 ;;
    esac
}
main "$@"
