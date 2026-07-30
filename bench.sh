#!/usr/bin/env bash
set -uo pipefail

usage(){ cat <<'TEXT'
Usage: bench.sh [MODE]

Measure this compiler: how fast it compiles, and how fast its output runs.
Where GNAT is available the same programs are built with it too and the two
are reported side by side. GNAT is installed automatically if it is missing.

Modes:
  codegen     run time of the generated code, at each optimisation level
  compile     time taken to compile, front end and whole build
  corpus      compile throughput over the conformance suite
  all         every mode (default)
  help        display this help and exit

Environment:
  REPEATS     timed repetitions after warmup (default: 7)
  WARMUP      untimed runs before measuring (default: 1)
  OPT         optimisation level for the compile and corpus modes (default: 2)
  CORPUS      files to take from the conformance suite (default: 300)
  NO_GNAT     set to 1 to skip GNAT entirely
  NO_ANIMATE  set to 1 to draw no progress indicators
  KEEP_WORK   set to 1 to keep the working tree

Each figure is the median of REPEATS timed runs, in seconds, taken after
WARMUP untimed runs. The value after it is the relative standard deviation
of the samples; a "!" in place of "±" marks a spread of 5% or more, where
the median should not be trusted without investigating. A ratio above 1.00
means this compiler is slower than GNAT. Every program's output is compared
between the two compilers and any disagreement is reported.
TEXT
}

case "${1:-}" in help|-h|--help) usage; exit 0 ;; esac

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
mode=${1:-all}
REPEATS=${REPEATS:-7}
WARMUP=${WARMUP:-1}
OPT=${OPT:-2}
CORPUS=${CORPUS:-300}
NO_GNAT=${NO_GNAT:-0}
NO_ANIMATE=${NO_ANIMATE:-0}
KEEP_WORK=${KEEP_WORK:-0}

ada83=$here/ada83
work=$(mktemp -d "${TMPDIR:-/tmp}/ada83-bench-XXXXXX")
PROGRAMS="sieve matmul recurse strings tasking"

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
    local label=$1
    ( local i=0 dim
      while :; do
        dim=$(( (i / 2) % 4 ))
        case $dim in
          0) printf '\r\033[2K  \033[2m◦ %s\033[0m' "$label" >&9 ;;
          1) printf '\r\033[2K  \033[2m◌ %s\033[0m' "$label" >&9 ;;
          2) printf '\r\033[2K  \033[2m◍ %s\033[0m' "$label" >&9 ;;
          3) printf '\r\033[2K  \033[2m◎ %s\033[0m' "$label" >&9 ;;
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

graph(){
    local label=$1 who=$2 value=$3 peak=$4 note=${5:-} width=30 filled
    filled=$(awk -v v="$value" -v m="$peak" -v w="$width" 'BEGIN{printf "%.3f",(m>0?v/m:0)*w}')
    printf '  %-9s %-6s %s %7s  %s\n' "$label" "$who" "$(bar "$filled" "$width")" "$value" "$note"
}

speedup(){
    awk -v a="$1" -v g="$2" 'BEGIN{
        if (a+0<=0 || g+0<=0) { print ""; exit }
        if (g > a) printf "%.1fx faster", g/a
        else if (a > g) printf "%.1fx slower", a/g
        else printf "level"
    }'
}

die(){ echo "bench.sh: $*" >&2; exit 1; }

ensure_ada83(){
    [ -x "$ada83" ] && return 0
    pulse "building the compiler"
    make -C "$here" -s ada83 >/dev/null 2>&1
    pulse_stop
    [ -x "$ada83" ] || die "cannot build $ada83"
}

sudo_if_needed(){ [ "$(id -u)" -eq 0 ] || echo sudo; }

install_gnat(){
    local s; s=$(sudo_if_needed)
    local cmd=""
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

write_programs(){
    mkdir -p "$work/src"
    cat > "$work/src/sieve.ada" <<'EOF'
with TEXT_IO; use TEXT_IO;
procedure SIEVE is
   LIMIT : constant := 2_000_000;
   type FLAGS is array (2 .. LIMIT) of BOOLEAN;
   PRIME : FLAGS;
   COUNT : INTEGER := 0;
begin
   for PASS in 1 .. 5 loop
      COUNT := 0;
      for I in PRIME'RANGE loop PRIME (I) := TRUE; end loop;
      for I in PRIME'RANGE loop
         if PRIME (I) then
            COUNT := COUNT + 1;
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
   N : constant := 400;
   type MATRIX is array (1 .. N, 1 .. N) of FLOAT;
   A, B, C : MATRIX;
   SUM : FLOAT;
begin
   for I in 1 .. N loop
      for J in 1 .. N loop
         A (I, J) := FLOAT (I + J);
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
   TOTAL : INTEGER := 0;
   function FIB (N : INTEGER) return INTEGER is
   begin
      if N < 2 then return N; end if;
      return FIB (N - 1) + FIB (N - 2);
   end FIB;
begin
   for I in 1 .. 6 loop
      TOTAL := TOTAL + FIB (32);
   end loop;
   PUT_LINE ("fib:" & INTEGER'IMAGE (TOTAL));
end RECURSE;
EOF
    cat > "$work/src/strings.ada" <<'EOF'
with TEXT_IO; use TEXT_IO;
procedure STRINGS is
   subtype LINE is STRING (1 .. 64);
   BUF   : LINE := (others => 'a');
   HITS  : INTEGER := 0;
   function COUNT_CHAR (S : STRING; C : CHARACTER) return INTEGER is
      N : INTEGER := 0;
   begin
      for I in S'RANGE loop
         if S (I) = C then N := N + 1; end if;
      end loop;
      return N;
   end COUNT_CHAR;
begin
   for PASS in 1 .. 3_000_000 loop
      BUF (1 + (PASS mod 64)) := CHARACTER'VAL (97 + (PASS mod 26));
      HITS := HITS + COUNT_CHAR (BUF (1 .. 32), 'a');
   end loop;
   PUT_LINE ("hits:" & INTEGER'IMAGE (HITS));
end STRINGS;
EOF
    cat > "$work/src/tasking.ada" <<'EOF'
with TEXT_IO; use TEXT_IO;
procedure TASKING is
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
   for I in 1 .. 200_000 loop
      SERVER.PUSH (1);
   end loop;
   SERVER.DRAIN (TOTAL);
   PUT_LINE ("rendezvous:" & INTEGER'IMAGE (TOTAL));
end TASKING;
EOF
    local p
    for p in $PROGRAMS; do cp "$work/src/$p.ada" "$work/src/$p.adb"; done
}

measure(){
    local samples="" t i
    for ((i = 0; i < WARMUP; i++)); do "$@" >/dev/null 2>&1; done
    for ((i = 0; i < REPEATS; i++)); do
        TIMEFORMAT=%R
        t=$( { time "$@" >/dev/null 2>&1; } 2>&1 )
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
            sd = (n>1) ? sqrt(spread/(n-1)) : 0
            printf "%.3f %.3f %.1f", median, v[1], (mean>0 ? sd*100/mean : 0)
        }'
}

median_of(){ set -- $1; printf '%s' "${1:-x}"; }
rsd_of(){ set -- $1; printf '%s' "${3:-0}"; }

dispersion(){
    local r=$1
    case $r in x) printf '%6s' "" ; return ;; esac
    awk -v r="$r" 'BEGIN{ printf (r>=5.0) ? "  !%3.0f%%" : "  ±%3.0f%%", r }'
}

ratio(){
    case "$1$2" in *x*) printf '%7s' "-"; return ;; esac
    awk -v a="$1" -v b="$2" 'BEGIN{ if (b+0==0) printf "%7s","-"; else printf "%7.2f",a/b }'
}

heading(){ printf '\n  \033[1m%s\033[0m\n  %s\n\n' "$1" "$2"; }
rule(){ printf '  %s\n' "────────────────────────────────────────────────────────────"; }

build_ada83(){ "$ada83" "-O$2" "$work/src/$1.ada" -o "$work/$1.a83"; }
build_gnat(){ ( cd "$work/src" && gnatmake -q "-O$2" "$1.adb" -o "$work/$1.gnat" ); }

run_codegen(){
    local gnat=$1 p level a g out_a out_g step=0 total
    total=$(( $(printf '%s\n' $PROGRAMS | wc -l) * 3 ))
    heading "GENERATED CODE" "run time of the compiled program"
    if [ "$gnat" = "1" ]; then
        printf '  %-9s %5s %14s %14s %8s\n' program opt ada83 gnat ratio
    else
        printf '  %-9s %5s %14s\n' program opt ada83
    fi
    rule
    for p in $PROGRAMS; do
        for level in 0 2 3; do
            step=$((step+1))
            progress "$((step-1))" "$total" "$p -O$level"
            if ! build_ada83 "$p" "$level" >/dev/null 2>&1; then
                progress_done
                printf '  %-9s %5s %9s\n' "$p" "-O$level" "build"; continue
            fi
            out_a=$("$work/$p.a83" 2>&1 | head -1)
            local sa; sa=$(measure "$work/$p.a83")
            a=$(median_of "$sa"); ar=$(rsd_of "$sa")
            if [ "$gnat" = "1" ]; then
                if build_gnat "$p" "$level" >/dev/null 2>&1; then
                    out_g=$("$work/$p.gnat" 2>&1 | head -1)
                    local sg; sg=$(measure "$work/$p.gnat")
                    g=$(median_of "$sg"); gr=$(rsd_of "$sg")
                else
                    g=x; gr=0; out_g=$out_a
                fi
                progress_done
                printf '  %-9s %5s %8s%s %8s%s %s' "$p" "-O$level" \
                    "$a" "$(dispersion "$ar")" "$g" "$(dispersion "$gr")" "$(ratio "$a" "$g")"
                [ "$out_a" = "$out_g" ] || printf '   OUTPUT DIFFERS'
                printf '\n'
            else
                progress_done
                printf '  %-9s %5s %8s%s\n' "$p" "-O$level" "$a" "$(dispersion "$ar")"
            fi
            [ "$level" = "2" ] && eval "T_$p=\$a" && eval "G_$p=\${g:-x}"
        done
    done
    progress_done
    codegen_graph "$gnat"
}

codegen_graph(){
    local gnat=$1 p a g peak=0
    heading "AT -O2, SIDE BY SIDE" "bar length is time; shorter is faster"
    for p in $PROGRAMS; do
        eval "a=\${T_$p:-x}"; eval "g=\${G_$p:-x}"
        case $a in x) ;; *) awk -v v="$a" -v m="$peak" 'BEGIN{exit !(v>m)}' && peak=$a ;; esac
        [ "$gnat" = "1" ] && case $g in x) ;; *) awk -v v="$g" -v m="$peak" 'BEGIN{exit !(v>m)}' && peak=$g ;; esac
    done
    for p in $PROGRAMS; do
        eval "a=\${T_$p:-x}"; eval "g=\${G_$p:-x}"
        [ "$a" = "x" ] && continue
        if [ "$gnat" = "1" ] && [ "$g" != "x" ]; then
            graph "$p" "ada83" "$a" "$peak" "$(speedup "$a" "$g")"
            graph "" "gnat" "$g" "$peak" ""
        else
            graph "$p" "ada83" "$a" "$peak" ""
        fi
    done
}

run_compile(){
    local gnat=$1 p a g f step=0 total
    total=$(printf '%s\n' $PROGRAMS | wc -l)
    heading "COMPILE TIME" "front end emits LLVM IR; build adds optimisation, code generation and linking"
    if [ "$gnat" = "1" ]; then
        printf '  %-9s %10s %14s %14s %8s\n' program "a83 front" "a83 build" "gnat build" ratio
    else
        printf '  %-9s %10s %14s\n' program "a83 front" "a83 build"
    fi
    rule
    for p in $PROGRAMS; do
        step=$((step+1))
        progress "$((step-1))" "$total" "compiling $p"
        local sf sa sg ar gr
        sf=$(measure "$ada83" --ir "$work/src/$p.ada" -o "$work/$p.ll"); f=$(median_of "$sf")
        sa=$(measure "$ada83" "-O$OPT" "$work/src/$p.ada" -o "$work/$p.a83")
        a=$(median_of "$sa"); ar=$(rsd_of "$sa")
        if [ "$gnat" = "1" ]; then
            sg=$(measure env sh -c "cd '$work/src' && rm -f *.ali *.o && gnatmake -q -O$OPT $p.adb -o '$work/$p.gnat'")
            g=$(median_of "$sg"); gr=$(rsd_of "$sg")
            progress_done
            printf '  %-9s %10s %8s%s %8s%s %s\n' "$p" "$f" \
                "$a" "$(dispersion "$ar")" "$g" "$(dispersion "$gr")" "$(ratio "$a" "$g")"
        else
            progress_done
            printf '  %-9s %10s %8s%s\n' "$p" "$f" "$a" "$(dispersion "$ar")"
        fi
    done
    progress_done
}

run_corpus(){
    heading "CORPUS THROUGHPUT" "the conformance suite compiled to LLVM IR"
    [ -d "$here/acats" ] || {
        [ -f "$here/tests.zip" ] || { echo "  no corpus available"; return; }
        pulse "unpacking the conformance suite"
        ( cd "$here" && unzip -q tests.zip )
        pulse_stop
    }
    local files count lines secs done_n=0
    files=$(ls "$here/acats"/*.ada 2>/dev/null | head -n "$CORPUS")
    [ -n "$files" ] || { echo "  no corpus available"; return; }
    count=$(printf '%s\n' $files | wc -l)
    lines=$(cat $files 2>/dev/null | wc -l)
    local f
    TIMEFORMAT=%R
    secs=$( { time { for f in $files; do
                "$ada83" --ir "$f" -o "$work/corpus.ll" >/dev/null 2>&1
                done_n=$((done_n+1))
                [ $((done_n % 10)) -eq 0 ] && progress "$done_n" "$count" "compiling the suite"
             done ; } ; } 2>&1 | tail -1 )
    progress_done
    printf '  %-22s %s\n' "files" "$count"
    printf '  %-22s %s\n' "lines" "$lines"
    printf '  %-22s %s s\n' "wall time" "$secs"
    awk -v l="$lines" -v s="$secs" 'BEGIN{ if (s+0>0) printf "  %-22s %d\n","lines per second",l/s }'
    awk -v c="$count" -v s="$secs" 'BEGIN{ if (s+0>0) printf "  %-22s %.1f\n","files per second",c/s }'
}

hide_cursor
ensure_ada83
write_programs
gnat=0
if have_gnat; then gnat=1; fi

printf '\n  \033[1mada83\033[0m   %s\n' "$("$ada83" --version 2>&1 | head -1)"
if [ "$gnat" = "1" ]; then
    printf '  \033[1mgnat\033[0m    %s\n' "$(gnatmake --version 2>&1 | head -1)"
else
    printf '  \033[1mgnat\033[0m    not available, reporting this compiler alone\n'
fi
cpu_model(){
    if [ -r /proc/cpuinfo ]; then
        sed -n 's/^model name[[:space:]]*: //p' /proc/cpuinfo | head -1
    else
        sysctl -n machdep.cpu.brand_string 2>/dev/null
    fi
}
printf '  \033[1mhost\033[0m    %s %s, %s cpus\n' "$(uname -s)" "$(uname -m)" \
    "$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo '?')"
[ -n "$(cpu_model)" ] && printf '  \033[1mcpu\033[0m     %s\n' "$(cpu_model)"
printf '  \033[1mmethod\033[0m  median of %s timed runs after %s warmup, ± is relative standard deviation\n' \
    "$REPEATS" "$WARMUP"

case "$mode" in
    codegen) run_codegen "$gnat" ;;
    compile) run_compile "$gnat" ;;
    corpus)  run_corpus ;;
    all)     run_codegen "$gnat"; run_compile "$gnat"; run_corpus ;;
    *)       show_cursor; echo "bench.sh: unknown mode '$mode'" >&2; usage >&2; exit 2 ;;
esac
show_cursor
echo
