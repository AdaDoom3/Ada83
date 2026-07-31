#!/usr/bin/env bash
set -uo pipefail

usage(){ cat <<'TEXT'
Usage: bench.sh [MODE] [ARGUMENT]

Measure this compiler so that work on ada83.c can be aimed, and then judged.

Modes:
  stages              where compile time goes: front end against back end
  corpus              throughput over the conformance suite, slowest inputs named
  compare REFERENCE   this compiler against another build of it, with deltas
  profile             the functions in ada83.c that compiling spends time in
  codegen             run time of the generated code, against GNAT where present
  memory              peak memory of the compiler and of what it produces
  all                 stages, corpus, codegen and memory
  help                display this help and exit

To judge a change to ada83.c, keep the old binary and name it:

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
  KEEP_WORK   set to 1 to keep the working tree

Each figure is the median of REPEATS timed runs, in seconds, after WARMUP
untimed runs, printed with the relative standard deviation of its samples.
A "!" in place of "±" marks a spread of 5% or more, where the median should
not be read as a result. Every program takes an opaque seed on its standard
input, so no loop in them can be folded away at compile time.
TEXT
}

case "${1:-}" in help|-h|--help) usage; exit 0 ;; esac

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
mode=${1:-all}
reference=${2:-}
REPEATS=${REPEATS:-7}
WARMUP=${WARMUP:-1}
OPT=${OPT:-2}
CORPUS=${CORPUS:-300}
SLOWEST=${SLOWEST:-12}
ONLY=${ONLY:-}
NO_GNAT=${NO_GNAT:-0}
NO_ANIMATE=${NO_ANIMATE:-0}
KEEP_WORK=${KEEP_WORK:-0}

ada83=$here/ada83
work=$(mktemp -d "${TMPDIR:-/tmp}/ada83-bench-XXXXXX")
seed=$work/seed

ALL_PROGRAMS="sieve matmul recurse strings numerics checks exceptions memory tasking"
PROGRAMS=${ONLY:-$ALL_PROGRAMS}

describe(){
    case $1 in
        sieve)      echo "integer arrays, index checks" ;;
        matmul)     echo "floating point, nested loops" ;;
        recurse)    echo "call and return" ;;
        strings)    echo "slices and character work" ;;
        numerics)   echo "fixed point and 12-digit float" ;;
        checks)     echo "range and index checks in a hot loop" ;;
        exceptions) echo "raise, propagate, handle" ;;
        memory)     echo "allocation and deallocation" ;;
        tasking)    echo "rendezvous" ;;
        *)          echo "" ;;
    esac
}

comparable(){ case $1 in numerics) return 1 ;; *) return 0 ;; esac; }

animated=0
[ -t 2 ] && [ "$NO_ANIMATE" != "1" ] && animated=1
if [ "$animated" = "1" ] && exec 9>/dev/tty 2>/dev/null; then :; else exec 9>/dev/null; animated=0; fi

cleanup(){ show_cursor; [ "$KEEP_WORK" = "1" ] || rm -rf "$work"; }
trap cleanup EXIT
trap 'show_cursor; exit 130' INT

hide_cursor(){ [ "$animated" = "1" ] && printf '\033[?25l' >&9; return 0; }
show_cursor(){ [ "$animated" = "1" ] && printf '\033[?25h' >&9; return 0; }

FRACTION_CHARS=("" "▏" "▎" "▍" "▌" "▋" "▊" "▉")

bar(){
    local filled=$1 width=$2 out="" full part i
    full=${filled%.*}
    part=$(awk -v f="$filled" -v w="$full" 'BEGIN{printf "%d",(f-w)*8}')
    for ((i = 0; i < full; i++)); do out+="█"; done
    if [ "$full" -lt "$width" ] && [ "$part" -gt 0 ]; then
        out+="${FRACTION_CHARS[$part]}"
        full=$((full+1))
    fi
    for ((i = full; i < width; i++)); do out+="·"; done
    printf '%s' "$out"
}

progress(){
    [ "$animated" = "1" ] || return 0
    local done=$1 total=$2 label=$3 width=32 filled pct
    filled=$(awk -v d="$done" -v t="$total" -v w="$width" 'BEGIN{printf "%.3f",(t>0?d/t:0)*w}')
    pct=$(awk -v d="$done" -v t="$total" 'BEGIN{printf "%3d",(t>0?d*100/t:0)}')
    printf '\r\033[2K  \033[2m%s\033[0m %s%%  \033[2m%s\033[0m' \
        "$(bar "$filled" "$width")" "$pct" "$label" >&9
}

progress_done(){ [ "$animated" = "1" ] && printf '\r\033[2K' >&9; return 0; }

PULSE_PID=""
pulse(){
    [ "$animated" = "1" ] || return 0
    ( local i=0
      while :; do
        case $(( (i / 2) % 4 )) in
          0) printf '\r\033[2K  \033[2m◦ %s\033[0m' "$1" >&9 ;;
          1) printf '\r\033[2K  \033[2m◌ %s\033[0m' "$1" >&9 ;;
          2) printf '\r\033[2K  \033[2m◍ %s\033[0m' "$1" >&9 ;;
          3) printf '\r\033[2K  \033[2m◎ %s\033[0m' "$1" >&9 ;;
        esac
        i=$((i+1)); sleep 0.12
      done ) &
    PULSE_PID=$!
}

pulse_stop(){
    [ -n "$PULSE_PID" ] || return 0
    kill "$PULSE_PID" 2>/dev/null; wait "$PULSE_PID" 2>/dev/null
    PULSE_PID=""
    [ "$animated" = "1" ] && printf '\r\033[2K' >&9
    return 0
}

heading(){ printf '\n  \033[1m%s\033[0m\n  %s\n\n' "$1" "$2"; }
rule(){ printf '  %s\n' "────────────────────────────────────────────────────────────────"; }
die(){ show_cursor; echo "bench.sh: $*" >&2; exit 1; }

measure(){
    local samples="" t i
    for ((i = 0; i < WARMUP; i++)); do "$@" <"$seed" >/dev/null 2>&1; done
    for ((i = 0; i < REPEATS; i++)); do
        TIMEFORMAT=%R
        t=$( { time "$@" <"$seed" >/dev/null 2>&1; } 2>&1 )
        case $t in ''|*[!0-9.]*) continue ;; esac
        samples="$samples$t\n"
    done
    [ -n "$samples" ] || { printf 'x x x'; return; }
    printf "$samples" | awk '
        {v[NR]=$1; total+=$1}
        END{
            n=NR; mean=total/n
            for (i=1;i<=n;i++) for (j=i+1;j<=n;j++) if (v[j]<v[i]) {t=v[i];v[i]=v[j];v[j]=t}
            median = (n%2) ? v[(n+1)/2] : (v[n/2]+v[n/2+1])/2
            for (i=1;i<=n;i++) spread += (v[i]-mean)*(v[i]-mean)
            printf "%.3f %.3f %.1f", median, v[1], (mean>0 ? sqrt(spread/(n>1?n-1:1))*100/mean : 0)
        }'
}

median_of(){ set -- $1; printf '%s' "${1:-x}"; }
rsd_of(){ set -- $1; printf '%s' "${3:-0}"; }

dispersion(){
    case $1 in x|'') printf '%6s' "" ; return ;; esac
    awk -v r="$1" 'BEGIN{ printf (r>=5.0) ? "  !%3.0f%%" : "  ±%3.0f%%", r }'
}

ratio(){
    case "$1$2" in *x*) printf '%7s' "-"; return ;; esac
    awk -v a="$1" -v b="$2" 'BEGIN{ if (b+0==0) printf "%7s","-"; else printf "%7.2f",a/b }'
}

delta(){
    case "$1$2" in *x*) printf '%9s' "-"; return ;; esac
    awk -v new="$1" -v old="$2" 'BEGIN{
        if (old+0==0) { printf "%9s","-"; exit }
        printf "%+8.1f%%", (new-old)*100/old
    }'
}

verdict(){
    case "$1$2" in *x*) printf ''; return ;; esac
    awk -v new="$1" -v old="$2" -v rn="$3" -v ro="$4" 'BEGIN{
        change=(new-old)*100/old
        noise=(rn>ro?rn:ro)
        if (change < -noise) printf "faster"
        else if (change > noise) printf "slower"
        else printf "level"
    }'
}

peak_rss(){
    local out
    if out=$(/usr/bin/time -v "$@" <"$seed" 2>&1 >/dev/null); then
        printf '%s' "$out" | awk '/Maximum resident set size/{printf "%.1f", $NF/1024}'
    elif out=$(/usr/bin/time -l "$@" <"$seed" 2>&1 >/dev/null); then
        printf '%s' "$out" | awk '/maximum resident set size/{printf "%.1f", $1/1048576}'
    else
        printf 'x'
    fi
}

sudo_if_needed(){ [ "$(id -u)" -eq 0 ] || echo sudo; }

install_gnat(){
    local s cmd=""; s=$(sudo_if_needed)
    if   command -v apt-get >/dev/null 2>&1; then cmd="$s apt-get install -y --no-install-recommends gnat"
    elif command -v dnf     >/dev/null 2>&1; then cmd="$s dnf install -y gcc-gnat"
    elif command -v pacman  >/dev/null 2>&1; then cmd="$s pacman -S --noconfirm gcc-ada"
    elif command -v zypper  >/dev/null 2>&1; then cmd="$s zypper install -y gcc-ada"
    elif command -v apk     >/dev/null 2>&1; then cmd="$s apk add gcc-gnat"
    elif command -v brew    >/dev/null 2>&1; then cmd="brew install gnat"
    else return 1
    fi
    pulse "installing GNAT to compare against"
    eval "$cmd" >/dev/null 2>&1
    pulse_stop
    command -v gnatmake >/dev/null 2>&1
}

have_gnat(){
    [ "$NO_GNAT" = "1" ] && return 1
    command -v gnatmake >/dev/null 2>&1 && return 0
    install_gnat
}

corpus_files(){ ls "$here/acats"/*.ada 2>/dev/null | head -n "$CORPUS"; }

corpus_ready(){
    [ -d "$here/acats" ] && return 0
    [ -f "$here/tests.zip" ] || return 1
    pulse "unpacking the conformance suite"
    ( cd "$here" && unzip -q tests.zip )
    pulse_stop
    [ -d "$here/acats" ]
}

write_programs(){
    mkdir -p "$work/src"
    echo 1 > "$seed"

    cat > "$work/src/sieve.ada" <<'EOF'
with TEXT_IO; use TEXT_IO;
procedure SIEVE is
   package INT_IO is new INTEGER_IO (INTEGER);
   LIMIT : constant := 2_000_000;
   type FLAGS is array (2 .. LIMIT) of BOOLEAN;
   SEED  : INTEGER;
   PRIME : FLAGS;
   COUNT : INTEGER := 0;
begin
   INT_IO.GET (SEED);
   for PASS in 1 .. 5 loop
      COUNT := 0;
      for I in PRIME'RANGE loop PRIME (I) := TRUE; end loop;
      for I in PRIME'RANGE loop
         if PRIME (I) then
            COUNT := COUNT + SEED;
            declare
               J : INTEGER := I * 2;
            begin
               while J <= LIMIT loop
                  PRIME (J) := FALSE;
                  J := J + I;
               end loop;
            end;
         end if;
      end loop;
   end loop;
   PUT_LINE ("primes:" & INTEGER'IMAGE (COUNT));
end SIEVE;
EOF

    cat > "$work/src/matmul.ada" <<'EOF'
with TEXT_IO; use TEXT_IO;
procedure MATMUL is
   package INT_IO is new INTEGER_IO (INTEGER);
   N : constant := 400;
   type MATRIX is array (1 .. N, 1 .. N) of FLOAT;
   SEED    : INTEGER;
   A, B, C : MATRIX;
   SUM     : FLOAT;
begin
   INT_IO.GET (SEED);
   for I in 1 .. N loop
      for J in 1 .. N loop
         A (I, J) := FLOAT (I + J * SEED);
         B (I, J) := FLOAT (I - J);
         C (I, J) := 0.0;
      end loop;
   end loop;
   for I in 1 .. N loop
      for J in 1 .. N loop
         SUM := 0.0;
         for K in 1 .. N loop
            SUM := SUM + A (I, K) * B (K, J);
         end loop;
         C (I, J) := SUM;
      end loop;
   end loop;
   SUM := 0.0;
   for I in 1 .. N loop
      for J in 1 .. N loop
         SUM := SUM + C (I, J);
      end loop;
   end loop;
   PUT_LINE ("checksum:" & INTEGER'IMAGE (INTEGER (SUM / 1.0E9)));
end MATMUL;
EOF

    cat > "$work/src/recurse.ada" <<'EOF'
with TEXT_IO; use TEXT_IO;
procedure RECURSE is
   package INT_IO is new INTEGER_IO (INTEGER);
   SEED  : INTEGER;
   TOTAL : INTEGER := 0;
   function FIB (N : INTEGER) return INTEGER is
   begin
      if N < 2 then return N; end if;
      return FIB (N - 1) + FIB (N - 2);
   end FIB;
begin
   INT_IO.GET (SEED);
   for I in 1 .. 6 loop
      TOTAL := TOTAL + FIB (30 + SEED);
   end loop;
   PUT_LINE ("fib:" & INTEGER'IMAGE (TOTAL));
end RECURSE;
EOF

    cat > "$work/src/strings.ada" <<'EOF'
with TEXT_IO; use TEXT_IO;
procedure STRINGS is
   package INT_IO is new INTEGER_IO (INTEGER);
   subtype LINE is STRING (1 .. 64);
   SEED : INTEGER;
   BUF  : LINE := (others => 'a');
   HITS : INTEGER := 0;
   function COUNT_CHAR (S : STRING; C : CHARACTER) return INTEGER is
      N : INTEGER := 0;
   begin
      for I in S'RANGE loop
         if S (I) = C then N := N + 1; end if;
      end loop;
      return N;
   end COUNT_CHAR;
begin
   INT_IO.GET (SEED);
   for PASS in 1 .. 3_000_000 loop
      BUF (1 + ((PASS * SEED) mod 64)) := CHARACTER'VAL (97 + (PASS mod 26));
      HITS := HITS + COUNT_CHAR (BUF (1 .. 32), 'a');
   end loop;
   PUT_LINE ("hits:" & INTEGER'IMAGE (HITS));
end STRINGS;
EOF

    cat > "$work/src/numerics.ada" <<'EOF'
with TEXT_IO; use TEXT_IO;
procedure NUMERICS is
   package INT_IO is new INTEGER_IO (INTEGER);
   type MONEY is delta 0.01 range -1_000_000.0 .. 1_000_000.0;
   type ANGLE is digits 12 range -1.0E9 .. 1.0E9;
   SEED  : INTEGER;
   ACC   : MONEY := 0.0;
   RATE  : MONEY := 0.07;
   THETA : ANGLE := 0.0;
   TALLY : INTEGER := 0;
begin
   INT_IO.GET (SEED);
   for I in 1 .. 40_000_000 loop
      ACC := ACC + RATE * SEED;
      if ACC > 900_000.0 then ACC := 0.0; end if;
      THETA := THETA + ANGLE (I mod 1024) * 1.0E-3;
      if THETA > 9.0E8 then THETA := 0.0; end if;
   end loop;
   TALLY := INTEGER (ACC) / 1000 + INTEGER (THETA / 1.0E6);
   PUT_LINE ("numerics:" & INTEGER'IMAGE (TALLY));
end NUMERICS;
EOF

    cat > "$work/src/checks.ada" <<'EOF'
with TEXT_IO; use TEXT_IO;
procedure CHECKS is
   package INT_IO is new INTEGER_IO (INTEGER);
   subtype SMALL is INTEGER range 0 .. 999;
   type TABLE is array (SMALL) of SMALL;
   SEED  : INTEGER;
   T     : TABLE := (others => 0);
   IDX   : SMALL := 0;
   TALLY : INTEGER := 0;
begin
   INT_IO.GET (SEED);
   for PASS in 1 .. 60_000 loop
      for I in SMALL loop
         IDX := SMALL ((I * 7 + PASS * SEED) mod 1000);
         T (IDX) := SMALL ((T (IDX) + I + PASS + SEED) mod 997);
      end loop;
   end loop;
   for I in SMALL loop TALLY := (TALLY + T (I)) mod 1_000_003; end loop;
   PUT_LINE ("checks:" & INTEGER'IMAGE (TALLY));
end CHECKS;
EOF

    cat > "$work/src/exceptions.ada" <<'EOF'
with TEXT_IO; use TEXT_IO;
procedure EXCEPTIONS is
   package INT_IO is new INTEGER_IO (INTEGER);
   TROUBLE : exception;
   SEED    : INTEGER;
   CAUGHT  : INTEGER := 0;
   procedure DEEP (LEVEL : INTEGER) is
   begin
      if LEVEL <= 0 then raise TROUBLE; end if;
      DEEP (LEVEL - 1);
   end DEEP;
begin
   INT_IO.GET (SEED);
   for I in 1 .. 2_000_000 loop
      begin
         DEEP (8 * SEED);
      exception
         when TROUBLE => CAUGHT := CAUGHT + 1;
      end;
   end loop;
   PUT_LINE ("caught:" & INTEGER'IMAGE (CAUGHT));
end EXCEPTIONS;
EOF

    cat > "$work/src/memory.ada" <<'EOF'
with TEXT_IO, UNCHECKED_DEALLOCATION; use TEXT_IO;
procedure MEMORY is
   package INT_IO is new INTEGER_IO (INTEGER);
   type NODE;
   type LINK is access NODE;
   type NODE is record
      VALUE : INTEGER;
      NEXT  : LINK;
   end record;
   procedure FREE is new UNCHECKED_DEALLOCATION (NODE, LINK);
   SEED  : INTEGER;
   HEAD  : LINK;
   N     : LINK;
   TALLY : INTEGER := 0;
begin
   INT_IO.GET (SEED);
   for PASS in 1 .. 3_000 loop
      HEAD := null;
      for I in 1 .. 5_000 loop
         N := new NODE'(VALUE => I * SEED, NEXT => HEAD);
         HEAD := N;
      end loop;
      while HEAD /= null loop
         TALLY := (TALLY + HEAD.VALUE) mod 1_000_003;
         N := HEAD;
         HEAD := HEAD.NEXT;
         FREE (N);
      end loop;
   end loop;
   PUT_LINE ("memory:" & INTEGER'IMAGE (TALLY));
end MEMORY;
EOF

    cat > "$work/src/tasking.ada" <<'EOF'
with TEXT_IO; use TEXT_IO;
procedure TASKING is
   package INT_IO is new INTEGER_IO (INTEGER);
   SEED  : INTEGER;
   TOTAL : INTEGER := 0;
   task SERVER is
      entry PUSH (V : INTEGER);
      entry DRAIN (V : out INTEGER);
   end SERVER;
   task body SERVER is
      ACC : INTEGER := 0;
   begin
      loop
         select
            accept PUSH (V : INTEGER) do ACC := ACC + V; end PUSH;
         or
            accept DRAIN (V : out INTEGER) do V := ACC; end DRAIN;
            exit;
         end select;
      end loop;
   end SERVER;
begin
   INT_IO.GET (SEED);
   for I in 1 .. 200_000 loop
      SERVER.PUSH (SEED);
   end loop;
   SERVER.DRAIN (TOTAL);
   PUT_LINE ("rendezvous:" & INTEGER'IMAGE (TOTAL));
end TASKING;
EOF

    local p
    for p in $ALL_PROGRAMS; do cp "$work/src/$p.ada" "$work/src/$p.adb"; done
}

run_stages(){
    local p front whole step=0 total
    total=$(printf '%s\n' $PROGRAMS | wc -l)
    heading "WHERE COMPILE TIME GOES" \
        "the front end emits IR; the rest is the LLVM pipeline, code generation and the linker"
    printf '  %-11s %11s %11s %11s %9s\n' program "front end" whole "back end" "front %"
    rule
    for p in $PROGRAMS; do
        step=$((step+1)); progress "$((step-1))" "$total" "staging $p"
        front=$(median_of "$(measure "$ada83" --ir "$work/src/$p.ada" -o "$work/$p.ll")")
        whole=$(median_of "$(measure "$ada83" "-O$OPT" "$work/src/$p.ada" -o "$work/$p.exe")")
        progress_done
        awk -v p="$p" -v f="$front" -v w="$whole" 'BEGIN{
            back = w - f
            printf "  %-11s %11s %11s %11.3f %8.0f%%\n", p, f, w, (back>0?back:0), (w>0? f*100/w : 0)
        }'
    done
    progress_done
    printf '\n  Where the front end is the smaller share, changes to ada83.c will not\n'
    printf '  show here; run again with OPT=0 to take the LLVM pipeline out of the way.\n'
}

run_corpus(){
    heading "CORPUS THROUGHPUT" "the conformance suite compiled to LLVM IR"
    corpus_ready || { echo "  no corpus available"; return; }
    local files count lines secs f done_n=0 one
    files=$(corpus_files)
    [ -n "$files" ] || { echo "  no corpus available"; return; }
    count=$(printf '%s\n' $files | wc -l)
    lines=$(cat $files 2>/dev/null | wc -l)
    : > "$work/times"
    TIMEFORMAT=%R
    secs=$( { time { for f in $files; do
                one=$( { time "$ada83" --ir "$f" -o "$work/c.ll" >/dev/null 2>&1; } 2>&1 )
                printf '%s\t%s\n' "$one" "$(basename "$f")" >> "$work/times"
                done_n=$((done_n+1))
                [ $((done_n % 10)) -eq 0 ] && progress "$done_n" "$count" "compiling the suite"
             done ; } ; } 2>&1 | tail -1 )
    progress_done
    printf '  %-22s %s\n' "files" "$count"
    printf '  %-22s %s\n' "lines" "$lines"
    printf '  %-22s %s s\n' "wall time" "$secs"
    awk -v l="$lines" -v s="$secs" 'BEGIN{ if (s+0>0) printf "  %-22s %d\n","lines per second",l/s }'
    awk -v c="$count" -v s="$secs" 'BEGIN{ if (s+0>0) printf "  %-22s %.1f\n","files per second",c/s }'
    heading "SLOWEST INPUTS" "the files worth opening a profiler on"
    local peak filled
    peak=$(sort -rn "$work/times" | head -1 | cut -f1)
    sort -rn "$work/times" | head -n "$SLOWEST" | while IFS=$'\t' read -r t name; do
        filled=$(awk -v v="$t" -v m="$peak" 'BEGIN{printf "%.3f",(m>0?v/m:0)*24}')
        printf '  %-18s %7ss  %s\n' "$name" "$t" "$(bar "$filled" 24)"
    done
}

run_compare(){
    local other=$1 p a b ra rb sa sb step=0 total f
    [ -n "$other" ] || die "compare needs the path of another ada83 binary"
    [ -x "$other" ] || die "cannot run $other"
    total=$(printf '%s\n' $PROGRAMS | wc -l)
    heading "AGAINST $(basename "$other")" \
        "compile time; negative is faster than the reference, and a change inside the noise reads level"
    printf '  %-11s %14s %14s %10s   %s\n' program reference "this build" delta ""
    rule
    for p in $PROGRAMS; do
        step=$((step+1)); progress "$((step-1))" "$total" "compiling $p"
        sb=$(measure "$other" "-O$OPT" "$work/src/$p.ada" -o "$work/$p.ref")
        sa=$(measure "$ada83" "-O$OPT" "$work/src/$p.ada" -o "$work/$p.new")
        b=$(median_of "$sb"); rb=$(rsd_of "$sb")
        a=$(median_of "$sa"); ra=$(rsd_of "$sa")
        progress_done
        printf '  %-11s %8s%s %8s%s %s   %s\n' "$p" \
            "$b" "$(dispersion "$rb")" "$a" "$(dispersion "$ra")" \
            "$(delta "$a" "$b")" "$(verdict "$a" "$b" "$ra" "$rb")"
    done
    progress_done
    corpus_ready || return 0
    local files ref new
    files=$(corpus_files)
    [ -n "$files" ] || return 0
    TIMEFORMAT=%R
    progress 1 2 "reference over the corpus"
    ref=$( { time { for f in $files; do "$other" --ir "$f" -o "$work/c.ll" >/dev/null 2>&1; done ; } ; } 2>&1 | tail -1 )
    progress 2 2 "this build over the corpus"
    new=$( { time { for f in $files; do "$ada83" --ir "$f" -o "$work/c.ll" >/dev/null 2>&1; done ; } ; } 2>&1 | tail -1 )
    progress_done
    rule
    printf '  %-11s %14s %14s %s\n' corpus "$ref" "$new" "$(delta "$new" "$ref")"
}

run_profile(){
    heading "WHERE THE COMPILER SPENDS ITS TIME" "symbols from ada83.c, compiling the corpus"
    corpus_ready || { echo "  no corpus available"; return; }
    local files f
    files=$(corpus_files)
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
    else
        echo "  no profile was produced"
    fi
}

run_codegen(){
    local gnat=$1 p a g sa sg out_a out_g peak=0 step=0 total
    total=$(printf '%s\n' $PROGRAMS | wc -l)
    heading "GENERATED CODE" "run time of the compiled program at -O$OPT"
    if [ "$gnat" = "1" ]; then
        printf '  %-11s %14s %14s %8s  %s\n' program ada83 gnat ratio stresses
    else
        printf '  %-11s %14s  %s\n' program ada83 stresses
    fi
    rule
    for p in $PROGRAMS; do
        step=$((step+1)); progress "$((step-1))" "$total" "$p"
        if ! "$ada83" "-O$OPT" "$work/src/$p.ada" -o "$work/$p.a83" >/dev/null 2>&1; then
            progress_done; printf '  %-11s %14s\n' "$p" "build failed"; continue
        fi
        sa=$(measure "$work/$p.a83"); a=$(median_of "$sa")
        out_a=$("$work/$p.a83" <"$seed" 2>&1 | head -1)
        eval "T_$p=\$a"
        if [ "$gnat" = "1" ]; then
            sg="x x x"; g=x; out_g=$out_a
            if ( cd "$work/src" && gnatmake -q "-O$OPT" "$p.adb" -o "$work/$p.gnat" ) >/dev/null 2>&1; then
                sg=$(measure "$work/$p.gnat"); g=$(median_of "$sg")
                out_g=$("$work/$p.gnat" <"$seed" 2>&1 | head -1)
            fi
            eval "G_$p=\$g"
            progress_done
            printf '  %-11s %8s%s %8s%s %s  %s' "$p" \
                "$a" "$(dispersion "$(rsd_of "$sa")")" "$g" "$(dispersion "$(rsd_of "$sg")")" \
                "$(ratio "$a" "$g")" "$(describe "$p")"
            if [ "$out_a" != "$out_g" ]; then
                if comparable "$p"; then printf '  OUTPUT DIFFERS'; else printf '  (representation differs)'; fi
            fi
            printf '\n'
        else
            progress_done
            printf '  %-11s %8s%s  %s\n' "$p" "$a" "$(dispersion "$(rsd_of "$sa")")" "$(describe "$p")"
        fi
    done
    progress_done
    [ "$gnat" = "1" ] || return 0
    heading "SIDE BY SIDE" "bar length is time; shorter is faster"
    for p in $PROGRAMS; do
        eval "a=\${T_$p:-x}"; eval "g=\${G_$p:-x}"
        case $a in x) ;; *) awk -v v="$a" -v m="$peak" 'BEGIN{exit !(v>m)}' && peak=$a ;; esac
        case $g in x) ;; *) awk -v v="$g" -v m="$peak" 'BEGIN{exit !(v>m)}' && peak=$g ;; esac
    done
    local fa fg note
    for p in $PROGRAMS; do
        eval "a=\${T_$p:-x}"; eval "g=\${G_$p:-x}"
        [ "$a" = "x" ] && continue
        fa=$(awk -v v="$a" -v m="$peak" 'BEGIN{printf "%.3f",(m>0?v/m:0)*30}')
        note=""
        [ "$g" != "x" ] && note=$(awk -v a="$a" -v g="$g" 'BEGIN{
            if (g>a) printf "%.1fx faster", g/a; else if (a>g) printf "%.1fx slower", a/g; else printf "level" }')
        printf '  %-11s %-6s %s %7s  %s\n' "$p" ada83 "$(bar "$fa" 30)" "$a" "$note"
        [ "$g" = "x" ] && continue
        fg=$(awk -v v="$g" -v m="$peak" 'BEGIN{printf "%.3f",(m>0?v/m:0)*30}')
        printf '  %-11s %-6s %s %7s\n' "" gnat "$(bar "$fg" 30)" "$g"
    done
}

run_memory(){
    local p compiling running step=0 total
    total=$(printf '%s\n' $PROGRAMS | wc -l)
    heading "PEAK MEMORY" "resident set at its high point, in megabytes"
    printf '  %-11s %14s %14s  %s\n' program "compiling it" "running it" stresses
    rule
    for p in $PROGRAMS; do
        step=$((step+1)); progress "$((step-1))" "$total" "$p"
        compiling=$(peak_rss "$ada83" "-O$OPT" "$work/src/$p.ada" -o "$work/$p.mem")
        running=x
        [ -x "$work/$p.mem" ] && running=$(peak_rss "$work/$p.mem")
        progress_done
        printf '  %-11s %14s %14s  %s\n' "$p" "${compiling:-x}" "${running:-x}" "$(describe "$p")"
    done
    progress_done
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
case "$mode" in codegen|all) have_gnat && gnat=1 ;; esac

printf '\n  \033[1mada83\033[0m   %s\n' "$("$ada83" --version 2>&1 | head -1)"
[ "$gnat" = "1" ] && printf '  \033[1mgnat\033[0m    %s\n' "$(gnatmake --version 2>&1 | head -1)"
printf '  \033[1mhost\033[0m    %s %s, %s cpus' "$(uname -s)" "$(uname -m)" \
    "$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo '?')"
[ -n "$(cpu_model)" ] && printf ', %s' "$(cpu_model)"
printf '\n  \033[1mmethod\033[0m  median of %s runs after %s warmup, at -O%s\n' "$REPEATS" "$WARMUP" "$OPT"

case "$mode" in
    stages)  run_stages ;;
    corpus)  run_corpus ;;
    compare) run_compare "$reference" ;;
    profile) run_profile ;;
    codegen) run_codegen "$gnat" ;;
    memory)  run_memory ;;
    all)     run_stages; run_corpus; run_codegen "$gnat"; run_memory ;;
    *)       show_cursor; echo "bench.sh: unknown mode '$mode'" >&2; usage >&2; exit 2 ;;
esac
show_cursor
echo
