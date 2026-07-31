#!/usr/bin/env bash
set -uo pipefail

usage(){ cat <<'TEXT'
Usage: bench.sh [MODE] [ARGUMENT]

Modes:
  stages              where compile time goes: front end against back end
  parser              front end against input size, to expose non-linear cost
  corpus              throughput over the conformance suite, slowest inputs named
  compare REFERENCE   this compiler against another build of it, with deltas
  profile             the functions in ada83.c that compiling spends time in
  codegen             run time of the generated code, against GNAT where present
  memory              peak memory of the compiler and of what it produces
  all                 every mode but compare and profile
  help                display this help and exit

To test keep the old binary and name it:

  cp ada83 /tmp/before && make && bash bench.sh compare /tmp/before

Environment:
  REPEATS     timed repetitions after warmup (default: 7)
  WARMUP      untimed runs before measuring (default: 1)
  OPT         optimisation level under test (default: 2)
  CORPUS      files to take from the conformance suite (default: 300)
  SLOWEST     slowest inputs to name in corpus mode (default: 12)
  ONLY        run only the named programs, space separated
  NO_GNAT     set to 1 to skip GNAT
  NO_ANIMATE  set to 1 to draw no progress indicators
  NO_COLOUR   set to 1 to draw no colour
  KEEP_WORK   set to 1 to keep the working tree

codegen mode only, where the two compilers are compared directly:
  CODEGEN_REPEATS  timed repetitions of each pair (default: 25)
  CODEGEN_WARMUP   untimed runs of each binary first (default: 3)
  SUITES           whole passes of the suite, to show the ratios hold (default: 2)
  BENCH_CPU        core to pin both binaries to (default: the last one)
  BENCH_CPUS_TASK  cores for the programs with tasks (default: the last two)
  RT               set to 1 to run at SCHED_FIFO priority 50 as well
  FLOOR            seconds below which a MAD is treated as timer noise (default: 0.002)
  LOAD_MAX         one-minute load average above which the run refuses (default: 2.0)
  FORCE            set to 1 to measure anyway on a busy machine
  TSV_DIR          directory to write suite<N>.tsv into, one row per program
  NO_PIN           set to 1 to measure without taskset, nice or chrt

TEXT
}

case "${1:-}" in help|-h|--help) usage; exit 0 ;; esac

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
mode=${1:-all} reference=${2:-}
REPEATS=${REPEATS:-7} WARMUP=${WARMUP:-1} OPT=${OPT:-2}
CORPUS=${CORPUS:-300} SLOWEST=${SLOWEST:-12} ONLY=${ONLY:-}
NO_GNAT=${NO_GNAT:-0} NO_ANIMATE=${NO_ANIMATE:-0} NO_COLOUR=${NO_COLOUR:-0}
KEEP_WORK=${KEEP_WORK:-0}
CODEGEN_REPEATS=${CODEGEN_REPEATS:-25} CODEGEN_WARMUP=${CODEGEN_WARMUP:-3}
SUITES=${SUITES:-2} RT=${RT:-0} FLOOR=${FLOOR:-0.002}
LOAD_MAX=${LOAD_MAX:-2.0} FORCE=${FORCE:-0} NO_PIN=${NO_PIN:-0} TSV_DIR=${TSV_DIR:-}

ada83=$here/ada83
work=$(mktemp -d "${TMPDIR:-/tmp}/ada83-bench-XXXXXX")
seed=$work/seed

ALL_PROGRAMS="sieve matmul lu recurse strings numerics checks exceptions memory tasking taskflood taskselect"
PROGRAMS=${ONLY:-$ALL_PROGRAMS}

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
    taskselect) echo "selective wait with an else part" ;;
esac; }

# Programs whose output must match GNAT's byte for byte. numerics is exempt:
# it accumulates a fixed point value forty million times, and Ada 83 lets an
# implementation choose its own `small` for such a type, so the two totals are
# both correct and different (132 against 720 here). Every other program is
# held to identical output, and a difference there is an error, not a footnote.
comparable(){ case $1 in numerics) return 1 ;; *) return 0 ;; esac; }

animated=0; [ -t 2 ] && [ "$NO_ANIMATE" != 1 ] && animated=1
if [ $animated = 1 ] && exec 9>/dev/tty 2>/dev/null; then :; else exec 9>/dev/null; animated=0; fi
if [ -t 1 ] && [ "$NO_COLOUR" != 1 ]
then BOLD=$'\033[1m' DIM=$'\033[2m' GOOD=$'\033[32m' BAD=$'\033[31m' OFF=$'\033[0m'
else BOLD='' DIM='' GOOD='' BAD='' OFF=''
fi

cleanup(){ show_cursor; [ "$KEEP_WORK" = 1 ] || rm -rf "$work"; }
trap cleanup EXIT
trap 'show_cursor; exit 130' INT

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
pulse(){
    [ $animated = 1 ] || return 0
    ( i=0; while :; do
        printf '\r\033[2K  %s%s %s%s' "$DIM" "$(printf '◦◌◍◎' | cut -c $(( (i/2)%4 + 1 )))" "$1" "$OFF" >&9
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
die(){ show_cursor; echo "bench.sh: $*" >&2; exit 1; }

measure(){
    local out='' t i
    for ((i = 0; i < WARMUP; i++)); do "$@" <"$seed" >/dev/null 2>&1; done
    for ((i = 0; i < REPEATS; i++)); do
        TIMEFORMAT=%R; t=$( { time "$@" <"$seed" >/dev/null 2>&1; } 2>&1 )
        case $t in ''|*[!0-9.]*) continue ;; esac
        out="$out$t\n"
    done
    [ -n "$out" ] || { printf 'x 0'; return; }
    printf "$out" | awk '{v[NR]=$1; s+=$1}
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

med(){ set -- $1; printf '%s' "${1:-x}"; }
rsd(){ set -- $1; printf '%s' "${2:-0}"; }

# ---------------------------------------------------------------------------
# Codegen measurement.
#
# The point of this mode is a ratio between two compilers, and a ratio is only
# worth printing when it is larger than the noise underneath it.  Four things
# are done about the noise, and one about what is claimed from it:
#
#   pinning       both binaries run on the same fixed cpus — one core for the
#                 programs with no tasks in them, a fixed pair for the ones
#                 that have tasks — so the scheduler moving a process around
#                 the machine is no longer a variable, and neither is a core
#                 the host happens to be throttling
#   interleaving  A and B alternate within one pair, and the pair swaps order
#                 every repetition, so load drifting during the run lands on
#                 both compilers instead of on whichever went second
#   repetition    more samples, summarised by median and median absolute
#                 deviation, which a single slow run cannot drag around the way
#                 it drags a mean and a standard deviation
#   warmup        three untimed runs of each binary before any are kept, so
#                 first-touch page faults and cold file cache are not measured
#   load watching the load average is sampled while measuring, not only before,
#                 because on a shared machine the neighbour starts when it likes
#
#   the rule      a ratio is printed only when the medians differ by more than
#                 the sum of the two MADs.  Rows that fail it are reported as
#                 indistinguishable rather than ordered.
# ---------------------------------------------------------------------------

ncpu=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1)

# Which cpus this process is actually allowed on. Counting them is not enough:
# inside a container started with --cpuset-cpus=4,5 there are two cpus but they
# are numbered 4 and 5, and taskset -c 1 would simply fail — silently dropping
# the pinning that the whole method rests on. Ask the kernel for the numbers.
allowed_cpus(){
    local list part a b i out=''
    list=$(taskset -pc $$ 2>/dev/null | sed 's/.*: *//')
    [ -n "$list" ] || { for ((i = 0; i < ncpu; i++)); do out+="$i "; done; printf '%s' "$out"; return; }
    local IFS=','
    for part in $list; do
        case $part in
            *-*) a=${part%%-*}; b=${part##*-}
                 for ((i = a; i <= b; i++)); do out+="$i "; done ;;
            *)   out+="$part " ;;
        esac
    done
    printf '%s' "$out"
}
CPUS=($(allowed_cpus))
[ ${#CPUS[@]} -gt 0 ] || CPUS=(0)

BENCH_CPU=${BENCH_CPU:-${CPUS[${#CPUS[@]}-1]}}

# The programs that create Ada tasks cannot honestly be squeezed onto one core.
# A task that polls — `select ... else` is a polling loop — spends its whole
# timeslice spinning while the task it is waiting for sits behind it in the run
# queue, so a single core turns a rendezvous into a scheduler round trip and
# measures the scheduler rather than the code. They get a fixed *pair* of
# cores instead: still no wandering across the machine, still identical for
# both compilers, but concurrency still means something. Which programs those
# are is not a judgement call, it is which ones declare a task.
BENCH_CPUS_TASK=${BENCH_CPUS_TASK:-}
if [ -z "$BENCH_CPUS_TASK" ]; then
    if [ ${#CPUS[@]} -ge 2 ]
    then BENCH_CPUS_TASK="${CPUS[${#CPUS[@]}-2]},${CPUS[${#CPUS[@]}-1]}"
    else BENCH_CPUS_TASK=$BENCH_CPU; fi
fi

concurrent(){ case $1 in tasking|taskflood|taskselect) return 0 ;; *) return 1 ;; esac; }

PRIO=() PRIO_WHY=''
if [ "$NO_PIN" != 1 ]; then
    if [ "$RT" = 1 ] && command -v chrt >/dev/null 2>&1 && chrt -f 50 true 2>/dev/null; then
        PRIO=(chrt -f 50); PRIO_WHY=', SCHED_FIFO 50'
    elif command -v nice >/dev/null 2>&1 && nice -n -5 true 2>/dev/null; then
        PRIO=(nice -n -5); PRIO_WHY=', nice -5'
    fi
fi

pinset(){  # cpu list -> the wrapper that holds a run on it
    if [ "$NO_PIN" != 1 ] && command -v taskset >/dev/null 2>&1 && taskset -c "$1" true 2>/dev/null
    then printf 'taskset -c %s\n' "$1"; fi
}
PIN_ONE=($(pinset "$BENCH_CPU")) PIN_MANY=($(pinset "$BENCH_CPUS_TASK"))
PIN_ONE+=(${PRIO[@]+"${PRIO[@]}"}); PIN_MANY+=(${PRIO[@]+"${PRIO[@]}"})
PIN=(${PIN_ONE[@]+"${PIN_ONE[@]}"})

if [ ${#PIN_ONE[@]} -eq 0 ]; then PIN_WHY='unpinned'
else PIN_WHY="pinned to cpu $BENCH_CPU, or cpus $BENCH_CPUS_TASK where the program has tasks${PRIO_WHY}"; fi

loadavg(){ [ -r /proc/loadavg ] && cut -d' ' -f1 </proc/loadavg || uptime 2>/dev/null | sed 's/.*average[s]*: *//;s/[, ].*//'; }

load_gate(){
    local l; l=$(loadavg); [ -n "$l" ] || return 0
    awk -v l="$l" -v m="$LOAD_MAX" 'BEGIN{exit !(l+0 > m+0)}' || return 0
    if [ "$FORCE" = 1 ]
    then printf '  %sload average is %s, above %s: measuring anyway (FORCE=1)%s\n' "$BAD" "$l" "$LOAD_MAX" "$OFF"
    else die "load average is $l, above LOAD_MAX=$LOAD_MAX; wait for the machine to settle or set FORCE=1"; fi
}

# One timed run, through the pinning wrapper. Prints seconds, or fails.
timed_run(){
    local t
    TIMEFORMAT=%R
    t=$( { time ${PIN[@]+"${PIN[@]}"} "$@" <"$seed" >/dev/null 2>&1; } 2>&1 )
    case $t in ''|*[!0-9.]*) return 1 ;; esac
    printf '%s\n' "$t"
}

# median, median absolute deviation and sample count of a file of seconds
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

# A one-minute load average taken before the run says nothing about what the
# machine did during it, and on a shared host something else can start at any
# moment. Sample it throughout, and report the worst the run saw.
LOAD_PEAK=0
load_watch_start(){
    ( while :; do loadavg; sleep 2; done > "$work/load.samples" 2>/dev/null ) &
    LOAD_WATCH=$!
}
# Sets LOAD_LAST to the worst load seen since load_watch_start, and carries
# the worst of the whole suite in LOAD_PEAK. Not a command substitution: that
# would run in a subshell and lose both.
load_watch_stop(){
    local pk
    LOAD_LAST=0
    [ -n "${LOAD_WATCH:-}" ] || return 0
    kill "$LOAD_WATCH" 2>/dev/null; wait "$LOAD_WATCH" 2>/dev/null; LOAD_WATCH=''
    pk=$(sort -g "$work/load.samples" 2>/dev/null | tail -1); [ -n "$pk" ] || pk=0
    LOAD_LAST=$pk
    awk -v p="$pk" -v m="$LOAD_PEAK" 'BEGIN{exit !(p+0 > m+0)}' && LOAD_PEAK=$pk
    return 0
}

# Two binaries, alternating, order swapped every repetition.
measure_pair(){
    local a=$1 b=$2 i t
    : > "$work/samples.a"; : > "$work/samples.b"
    for ((i = 0; i < CODEGEN_WARMUP; i++)); do
        ${PIN[@]+"${PIN[@]}"} "$a" <"$seed" >/dev/null 2>&1
        [ -n "$b" ] && ${PIN[@]+"${PIN[@]}"} "$b" <"$seed" >/dev/null 2>&1
    done
    for ((i = 0; i < CODEGEN_REPEATS; i++)); do
        if [ -z "$b" ]; then
            t=$(timed_run "$a") && printf '%s\n' "$t" >> "$work/samples.a"
        elif [ $((i % 2)) -eq 0 ]; then
            t=$(timed_run "$a") && printf '%s\n' "$t" >> "$work/samples.a"
            t=$(timed_run "$b") && printf '%s\n' "$t" >> "$work/samples.b"
        else
            t=$(timed_run "$b") && printf '%s\n' "$t" >> "$work/samples.b"
            t=$(timed_run "$a") && printf '%s\n' "$t" >> "$work/samples.a"
        fi
    done
    return 0
}

# The rule: the medians must differ by more than the sum of the two MADs,
# each MAD floored at the timer's own noise so a run that happens to repeat
# to the millisecond cannot make a millisecond difference look real.
distinguishable(){
    awk -v a="$1" -v am="$2" -v g="$3" -v gm="$4" -v f="$FLOOR" 'BEGIN{
        if (a == "x" || g == "x") exit 1
        if (am + 0 < f + 0) am = f
        if (gm + 0 < f + 0) gm = f
        d = a - g; if (d < 0) d = -d
        exit !(d > am + gm) }'
}

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

peak_rss(){
    local o
    if o=$(/usr/bin/time -v "$@" <"$seed" 2>&1 >/dev/null); then
        printf '%s' "$o" | awk '/Maximum resident set size/{printf "%.1f", $NF/1024}'
    elif o=$(/usr/bin/time -l "$@" <"$seed" 2>&1 >/dev/null); then
        printf '%s' "$o" | awk '/maximum resident set size/{printf "%.1f", $1/1048576}'
    else printf 'x'; fi
}

install_gnat(){
    local s c=''; [ "$(id -u)" -eq 0 ] || s=sudo
    for try in "apt-get:$s apt-get install -y --no-install-recommends gnat" \
               "dnf:$s dnf install -y gcc-gnat" "pacman:$s pacman -S --noconfirm gcc-ada" \
               "zypper:$s zypper install -y gcc-ada" "apk:$s apk add gcc-gnat" "brew:brew install gnat"; do
        command -v "${try%%:*}" >/dev/null 2>&1 && { c=${try#*:}; break; }
    done
    [ -n "$c" ] || return 1
    pulse "installing GNAT to compare against"; eval "$c" >/dev/null 2>&1; pulse_stop
    command -v gnatmake >/dev/null 2>&1
}

have_gnat(){
    [ "$NO_GNAT" = 1 ] && return 1
    command -v gnatmake >/dev/null 2>&1 || install_gnat
}

corpus_ready(){
    [ -d "$here/acats" ] && return 0
    [ -f "$here/tests.zip" ] || return 1
    pulse "unpacking the conformance suite"; ( cd "$here" && unzip -q tests.zip ); pulse_stop
    [ -d "$here/acats" ]
}
corpus_files(){ ls "$here/acats"/*.ada 2>/dev/null | head -n "$CORPUS"; }

count(){ printf '%s\n' $1 | wc -l; }

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
    mkdir -p "$work/src"; echo 1 > "$seed"

    cat > "$work/src/sieve.ada" <<'EOF'
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

    cat > "$work/src/matmul.ada" <<'EOF'
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

    cat > "$work/src/lu.ada" <<'EOF'
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

    cat > "$work/src/recurse.ada" <<'EOF'
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

    cat > "$work/src/strings.ada" <<'EOF'
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

    cat > "$work/src/numerics.ada" <<'EOF'
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

    cat > "$work/src/checks.ada" <<'EOF'
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

    cat > "$work/src/exceptions.ada" <<'EOF'
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

    cat > "$work/src/memory.ada" <<'EOF'
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

    cat > "$work/src/tasking.ada" <<'EOF'
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

    cat > "$work/src/taskflood.ada" <<'EOF'
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

    cat > "$work/src/taskselect.ada" <<'EOF'
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

    local p
    for p in $ALL_PROGRAMS; do cp "$work/src/$p.ada" "$work/src/$p.adb"; done
}

run_stages(){
    local p f w n=0 total; total=$(count "$PROGRAMS")
    heading "COMPILE STAGES"
    printf '  %-11s %11s %11s %11s %9s\n' program "front end" whole "back end" "front %"; rule
    for p in $PROGRAMS; do
        progress $((n++)) "$total" "staging $p"
        f=$(med "$(measure "$ada83" --ir "$work/src/$p.ada" -o "$work/$p.ll")")
        w=$(med "$(measure "$ada83" "-O$OPT" "$work/src/$p.ada" -o "$work/$p.exe")")
        clear_line
        awk -v p="$p" -v f="$f" -v w="$w" 'BEGIN{ b=w-f
            printf "  %-11s %11s %11s %11.3f %8.0f%%\n", p, f, w, (b>0?b:0), (w>0?f*100/w:0) }'
    done
    clear_line
}

run_parser(){
    local n t lines peak=0 first='' firstlines=''
    heading "FRONT END AGAINST INPUT SIZE"
    printf '  %-8s %9s %11s %13s %s\n' units lines "front end" "µs per line" ""; rule
    for n in 50 100 200 400 800; do
        progress "$n" 800 "generating and compiling ${n} units"
        monster "$n" > "$work/src/monster.ada"
        lines=$(wc -l < "$work/src/monster.ada")
        t=$(med "$(measure "$ada83" --ir "$work/src/monster.ada" -o "$work/monster.ll")")
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
    corpus_ready || { echo "  no corpus available"; return; }
    local files n lines secs f done=0 one peak
    files=$(corpus_files); [ -n "$files" ] || { echo "  no corpus available"; return; }
    n=$(count "$files"); lines=$(cat $files 2>/dev/null | wc -l); : > "$work/times"
    TIMEFORMAT=%R
    secs=$( { time { for f in $files; do
            one=$( { time "$ada83" --ir "$f" -o "$work/c.ll" >/dev/null 2>&1; } 2>&1 )
            printf '%s\t%s\n' "$one" "$(basename "$f")" >> "$work/times"
            done=$((done+1)); [ $((done % 10)) = 0 ] && progress "$done" "$n" "compiling the suite"
         done ; } ; } 2>&1 | tail -1 )
    clear_line
    printf '  %-22s %s\n' files "$n"
    printf '  %-22s %s\n' lines "$lines"
    printf '  %-22s %s s\n' "wall time" "$secs"
    awk -v l="$lines" -v s="$secs" 'BEGIN{if(s+0>0)printf "  %-22s %d\n","lines per second",l/s}'
    awk -v c="$n" -v s="$secs" 'BEGIN{if(s+0>0)printf "  %-22s %.1f\n","files per second",c/s}'
    heading "SLOWEST INPUTS"
    peak=$(sort -rn "$work/times" | head -1 | cut -f1)
    sort -rn "$work/times" | head -n "$SLOWEST" | while IFS=$'\t' read -r t name; do
        printf '  %-18s %7ss  %s\n' "$name" "$t" "$(bar "$(scaled "$t" "$peak" 24)" 24)"
    done
}

run_compare(){
    local other=$1 p a b sa sb n=0 total f ref new files
    [ -n "$other" ] || die "compare needs the path of another ada83 binary"
    [ -x "$other" ] || die "cannot run $other"
    total=$(count "$PROGRAMS")
    heading "AGAINST $(basename "$other")"
    printf '  %-11s %14s %14s %10s   %s\n' program reference "this build" delta ''; rule
    for p in $PROGRAMS; do
        progress $((n++)) "$total" "compiling $p"
        sb=$(measure "$other" "-O$OPT" "$work/src/$p.ada" -o "$work/$p.ref")
        sa=$(measure "$ada83" "-O$OPT" "$work/src/$p.ada" -o "$work/$p.new")
        b=$(med "$sb"); a=$(med "$sa"); clear_line
        printf '  %-11s %8s%s %8s%s %s   %s\n' "$p" \
            "$b" "$(spread "$(rsd "$sb")")" "$a" "$(spread "$(rsd "$sa")")" \
            "$(change "$a" "$b")" "$(verdict "$a" "$b" "$(rsd "$sa")" "$(rsd "$sb")")"
    done
    clear_line
    corpus_ready || return 0
    files=$(corpus_files); [ -n "$files" ] || return 0
    TIMEFORMAT=%R
    progress 1 2 "reference over the corpus"
    ref=$( { time { for f in $files; do "$other" --ir "$f" -o "$work/c.ll" >/dev/null 2>&1; done ; } ; } 2>&1 | tail -1 )
    progress 2 2 "this build over the corpus"
    new=$( { time { for f in $files; do "$ada83" --ir "$f" -o "$work/c.ll" >/dev/null 2>&1; done ; } ; } 2>&1 | tail -1 )
    clear_line; rule
    printf '  %-11s %14s %14s %s\n' corpus "$ref" "$new" "$(change "$new" "$ref")"
}

run_profile(){
    heading "PROFILE"
    corpus_ready || { echo "  no corpus available"; return; }
    local files f; files=$(corpus_files)
    [ -n "$files" ] || { echo "  no corpus available"; return; }
    if command -v perf >/dev/null 2>&1 && perf stat true >/dev/null 2>&1; then
        pulse "recording with perf"
        perf record -q -o "$work/perf.data" -- sh -c \
            'for f in '"$files"'; do "'"$ada83"'" --ir "$f" -o "'"$work"'/c.ll" >/dev/null 2>&1; done' >/dev/null 2>&1
        pulse_stop
        perf report -i "$work/perf.data" --stdio --no-children -F overhead,symbol 2>/dev/null \
            | grep -E '^\s+[0-9]' | head -20 | sed 's/^/  /'
        return
    fi
    pulse "building an instrumented compiler"
    gcc -O2 -pg -w -std=gnu2x -o "$work/ada83-pg" "$here/ada83.c" -lm -lpthread >/dev/null 2>&1
    pulse_stop
    [ -x "$work/ada83-pg" ] || { echo "  perf is absent and the instrumented build failed"; return; }
    pulse "profiling"
    ( cd "$work" && for f in $files; do "$work/ada83-pg" --ir "$f" -o "$work/c.ll" >/dev/null 2>&1; done )
    pulse_stop
    if [ -f "$work/gmon.out" ] && command -v gprof >/dev/null 2>&1; then
        ( cd "$work" && gprof -b -p "$work/ada83-pg" gmon.out 2>/dev/null | head -22 | sed 's/^/  /' )
    else echo "  no profile was produced"; fi
}

run_codegen(){
    local gnat=$1 suite=${2:-1} p a g am gm na ng oa og n=0 total peak=0
    local verdict r note tsv='' cpus=$BENCH_CPU lpk=0
    total=$(count "$PROGRAMS"); LOAD_PEAK=0
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
    for p in $PROGRAMS; do
        progress $((n++)) "$total" "$p"
        if ! "$ada83" "-O$OPT" "$work/src/$p.ada" -o "$work/$p.a83" >/dev/null 2>&1; then
            clear_line; printf '  %-11s %14s\n' "$p" "build failed"
            eval "T_$p=x G_$p=x R_${suite}_$p=- D_${suite}_$p=0"; continue
        fi
        g=x gm=x ng=0 og=''
        if concurrent "$p"
        then PIN=(${PIN_MANY[@]+"${PIN_MANY[@]}"}); cpus=$BENCH_CPUS_TASK
        else PIN=(${PIN_ONE[@]+"${PIN_ONE[@]}"});  cpus=$BENCH_CPU; fi
        load_watch_start
        if [ "$gnat" = 1 ] &&
           ( cd "$work/src" && gnatmake -q "-O$OPT" "$p.adb" -o "$work/$p.gnat" ) >/dev/null 2>&1
        then measure_pair "$work/$p.a83" "$work/$p.gnat"
             read -r g gm ng <<<"$(stats "$work/samples.b")"
        else measure_pair "$work/$p.a83" ''; fi
        load_watch_stop; lpk=$LOAD_LAST
        read -r a am na <<<"$(stats "$work/samples.a")"
        oa=$("$work/$p.a83" <"$seed" 2>&1 | head -1)
        [ "$g" = x ] || og=$("$work/$p.gnat" <"$seed" 2>&1 | head -1)
        eval "T_$p=\$a G_$p=\$g AM_$p=\$am GM_$p=\$gm"
        clear_line
        if [ "$gnat" = 1 ]; then
            note=same
            if [ "$g" != x ] && [ "$oa" != "$og" ]
            then comparable "$p" && note=DIFFERS || note=representation; fi
            if distinguishable "$a" "$am" "$g" "$gm"; then
                r=$(awk -v a="$a" -v b="$g" 'BEGIN{printf "%.2f", a/b}')
                verdict=$(speedup "$a" "$g")
                case $verdict in *faster) verdict="$GOOD$verdict$OFF" ;; *slower) verdict="$BAD$verdict$OFF" ;; esac
                eval "R_${suite}_$p=\$r D_${suite}_$p=1"
            else
                r='-'; verdict="${DIM}indistinguishable$OFF"
                eval "R_${suite}_$p=- D_${suite}_$p=0"
            fi
            eval "MA_${suite}_$p=\$a MG_${suite}_$p=\$g"
            printf '  %-11s %s %s %7s  %-17s %s' "$p" "$(pm "$a" "$am")" "$(pm "$g" "$gm")" \
                "$r" "$verdict" "$(describe "$p")"
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
    for p in $PROGRAMS; do
        eval "a=\${T_$p:-x}"; eval "g=\${G_$p:-x}"
        for v in "$a" "$g"; do
            case $v in x) ;; *) awk -v v="$v" -v m="$peak" 'BEGIN{exit !(v>m)}' && peak=$v ;; esac
        done
    done
    for p in $PROGRAMS; do
        eval "a=\${T_$p:-x}"; eval "g=\${G_$p:-x}"; eval "n=\${D_${suite}_$p:-0}"
        [ "$a" = x ] && continue
        verdict=$(speedup "$a" "$g"); [ "$n" = 1 ] || verdict='indistinguishable'
        printf '  %-11s %s%-5s%s %s %7s  %s\n' "$p" "$BOLD" ada83 "$OFF" \
            "$(bar "$(scaled "$a" "$peak" 30)" 30)" "$a" "$verdict"
        [ "$g" = x ] && continue
        printf '  %-11s %s%-5s%s %s %7s\n' '' "$DIM" gnat "$OFF" \
            "$(bar "$(scaled "$g" "$peak" 30)" 30)" "$g"
    done
    return 0
}

# Every suite is a full pass over every program. Two passes an hour apart in
# machine load are the check on the ratios themselves: if suite 2 disagrees
# with suite 1, the number is not stable enough to publish, whatever its MAD.
run_codegen_suites(){
    local gnat=$1 s
    load_gate
    for ((s = 1; s <= SUITES; s++)); do run_codegen "$gnat" "$s"; done
    [ "$SUITES" -gt 1 ] && [ "$gnat" = 1 ] && run_stability
    return 0
}

# The second pass is the only thing that can catch a ratio that is tight
# within a run and still not a property of the code — one that moves when the
# machine's mood does. A row that fails here should not be published, however
# small its MADs were.
run_stability(){
    local p r1 r2 d1 d2 a1 g1 a2 g2
    heading "SUITE AGAINST SUITE" "the same ratio measured twice, as the check that it is real"
    printf '  %-11s %9s %9s %10s  %s\n' program 'suite 1' 'suite 2' drift ''; rule
    for p in $PROGRAMS; do
        eval "r1=\${R_1_$p:-x} r2=\${R_2_$p:-x} d1=\${D_1_$p:-0} d2=\${D_2_$p:-0}"
        eval "a1=\${MA_1_$p:-x} g1=\${MG_1_$p:-x} a2=\${MA_2_$p:-x} g2=\${MG_2_$p:-x}"
        [ "$r1" = x ] && continue
        if [ "$d1" != 1 ] || [ "$d2" != 1 ]; then
            printf '  %-11s %9s %9s %10s  %s\n' "$p" "$r1" "$r2" '-' \
                "$([ "$d1" = "$d2" ] && echo 'indistinguishable in both suites' \
                   || echo 'DISTINGUISHABLE IN ONE SUITE ONLY — not published')"
            continue
        fi
        # from the medians rather than the printed two-decimal ratio, which at
        # 0.01 is coarser than the drift being looked for
        awk -v p="$p" -v a1="$a1" -v g1="$g1" -v a2="$a2" -v g2="$g2" 'BEGIN{
            r1 = a1 / g1; r2 = a2 / g2; d = (r2 - r1) * 100 / r1
            printf "  %-11s %9.3f %9.3f %+9.1f%%  %s\n", p, r1, r2, d,
                ((d < 0 ? -d : d) <= 10) ? "stable" \
                                         : "UNSTABLE — NO RATIO SHOULD BE PUBLISHED FOR THIS ROW" }'
    done
}

run_memory(){
    local p c r n=0 total; total=$(count "$PROGRAMS")
    heading "PEAK MEMORY" "megabytes"
    printf '  %-11s %14s %14s  %s\n' program "compiling it" "running it" stresses; rule
    for p in $PROGRAMS; do
        progress $((n++)) "$total" "$p"
        c=$(peak_rss "$ada83" "-O$OPT" "$work/src/$p.ada" -o "$work/$p.mem")
        r=x; [ -x "$work/$p.mem" ] && r=$(peak_rss "$work/$p.mem")
        clear_line
        printf '  %-11s %14s %14s  %s\n' "$p" "${c:-x}" "${r:-x}" "$(describe "$p")"
    done
    clear_line
}

cpu_model(){
    if [ -r /proc/cpuinfo ]; then sed -n 's/^model name[[:space:]]*: //p' /proc/cpuinfo | head -1
    else sysctl -n machdep.cpu.brand_string 2>/dev/null; fi
}

hide_cursor
[ -x "$ada83" ] || { pulse "building the compiler"; make -C "$here" -s ada83 >/dev/null 2>&1; pulse_stop; }
[ -x "$ada83" ] || die "cannot build $ada83"
write_programs

gnat=0
case $mode in codegen|all) have_gnat && gnat=1 ;; esac

# What the compiler under test was built with. The makefile can be asked, but
# a build that did not come from the makefile — the container image, say —
# sets ADA83_BUILD_FLAGS instead, so the line printed is never a guess.
build_flags(){
    [ -n "${ADA83_BUILD_FLAGS:-}" ] && { printf '%s\n' "$ADA83_BUILD_FLAGS"; return; }
    make -C "$here" -s --eval='bench-print-flags: ; @echo $(CC) $(CFLAGS) $(WHOLE_PROGRAM) $(TUNE)' \
        bench-print-flags 2>/dev/null | head -1
}

printf '\n  %sada83%s   %s\n' "$BOLD" "$OFF" "$("$ada83" --version 2>&1 | head -1)"
printf '  %sbuilt%s   %s\n' "$BOLD" "$OFF" "$(build_flags)"
[ $gnat = 1 ] && printf '  %sgnat%s    %s (%s)\n' "$BOLD" "$OFF" "$(gnatmake --version 2>&1 | head -1)" \
    "$(gcc --version 2>&1 | head -1)"
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

case $mode in
    stages)  run_stages ;;
    parser)  run_parser ;;
    corpus)  run_corpus ;;
    compare) run_compare "$reference" ;;
    profile) run_profile ;;
    codegen) run_codegen_suites "$gnat" ;;
    memory)  run_memory ;;
    all)     run_stages; run_parser; run_corpus; run_codegen_suites "$gnat"; run_memory ;;
    *)       show_cursor; echo "bench.sh: unknown mode '$mode'" >&2; usage >&2; exit 2 ;;
esac
show_cursor
echo
