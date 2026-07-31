#!/usr/bin/env bash
set -uo pipefail

usage(){ cat <<'TEXT'
Usage: bench.sh [MODE] [ARGUMENT]

Measure this compiler so that work on ada83.c can be aimed, and then judged.

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
  NO_COLOUR   set to 1 to draw no colour
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
mode=${1:-all} reference=${2:-}
REPEATS=${REPEATS:-7} WARMUP=${WARMUP:-1} OPT=${OPT:-2}
CORPUS=${CORPUS:-300} SLOWEST=${SLOWEST:-12} ONLY=${ONLY:-}
NO_GNAT=${NO_GNAT:-0} NO_ANIMATE=${NO_ANIMATE:-0} NO_COLOUR=${NO_COLOUR:-0}
KEEP_WORK=${KEEP_WORK:-0}

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

heading(){ printf '\n  %s%s%s\n  %s%s%s\n\n' "$BOLD" "$1" "$OFF" "$DIM" "$2" "$OFF"; }
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
             printf "%.3f %.1f", mid, (mean > 0) ? sd * 100 / mean : 0 }'
}

med(){ set -- $1; printf '%s' "${1:-x}"; }
rsd(){ set -- $1; printf '%s' "${2:-0}"; }

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
    heading "WHERE COMPILE TIME GOES" \
        "the front end emits IR; the rest is the LLVM pipeline, code generation and the linker"
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
    printf '\n  %sWhere the front end is the smaller share, changes to ada83.c will not show\n' "$DIM"
    printf '  here; run again with OPT=0 to take the LLVM pipeline out of the way.%s\n' "$OFF"
}

run_parser(){
    local n t lines peak=0 first='' firstlines=''
    heading "FRONT END AGAINST INPUT SIZE" \
        "a generated unit of types, records, case statements and nested expressions"
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
    heading "CORPUS THROUGHPUT" "the conformance suite compiled to LLVM IR"
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
    heading "SLOWEST INPUTS" "the files worth opening a profiler on"
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
    heading "AGAINST $(basename "$other")" \
        "compile time; negative is faster than the reference, and a change inside the noise reads level"
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
    heading "WHERE THE COMPILER SPENDS ITS TIME" "symbols from ada83.c, compiling the corpus"
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
    local gnat=$1 p a g sa sg oa og n=0 total peak=0 fa fg
    total=$(count "$PROGRAMS")
    heading "GENERATED CODE" "run time of the compiled program at -O$OPT"
    if [ "$gnat" = 1 ]
    then printf '  %-11s %14s %14s %8s  %s\n' program ada83 gnat ratio stresses
    else printf '  %-11s %14s  %s\n' program ada83 stresses; fi
    rule
    for p in $PROGRAMS; do
        progress $((n++)) "$total" "$p"
        if ! "$ada83" "-O$OPT" "$work/src/$p.ada" -o "$work/$p.a83" >/dev/null 2>&1; then
            clear_line; printf '  %-11s %14s\n' "$p" "build failed"; continue
        fi
        sa=$(measure "$work/$p.a83"); a=$(med "$sa")
        oa=$("$work/$p.a83" <"$seed" 2>&1 | head -1)
        eval "T_$p=\$a"
        if [ "$gnat" = 1 ]; then
            sg='x 0'; g=x; og=$oa
            if ( cd "$work/src" && gnatmake -q "-O$OPT" "$p.adb" -o "$work/$p.gnat" ) >/dev/null 2>&1; then
                sg=$(measure "$work/$p.gnat"); g=$(med "$sg"); og=$("$work/$p.gnat" <"$seed" 2>&1 | head -1)
            fi
            eval "G_$p=\$g"; clear_line
            printf '  %-11s %8s%s %8s%s %s  %s' "$p" "$a" "$(spread "$(rsd "$sa")")" \
                "$g" "$(spread "$(rsd "$sg")")" "$(ratio "$a" "$g")" "$(describe "$p")"
            [ "$oa" != "$og" ] && { comparable "$p" && printf '  %sOUTPUT DIFFERS%s' "$BAD" "$OFF" \
                                    || printf '  %s(representation differs)%s' "$DIM" "$OFF"; }
            printf '\n'
        else
            clear_line
            printf '  %-11s %8s%s  %s\n' "$p" "$a" "$(spread "$(rsd "$sa")")" "$(describe "$p")"
        fi
    done
    clear_line
    [ "$gnat" = 1 ] || return 0
    heading "SIDE BY SIDE" "bar length is time; shorter is faster"
    for p in $PROGRAMS; do
        eval "a=\${T_$p:-x}"; eval "g=\${G_$p:-x}"
        for v in "$a" "$g"; do
            case $v in x) ;; *) awk -v v="$v" -v m="$peak" 'BEGIN{exit !(v>m)}' && peak=$v ;; esac
        done
    done
    for p in $PROGRAMS; do
        eval "a=\${T_$p:-x}"; eval "g=\${G_$p:-x}"
        [ "$a" = x ] && continue
        printf '  %-11s %s%-5s%s %s %7s  %s\n' "$p" "$BOLD" ada83 "$OFF" \
            "$(bar "$(scaled "$a" "$peak" 30)" 30)" "$a" "$(speedup "$a" "$g")"
        [ "$g" = x ] && continue
        printf '  %-11s %s%-5s%s %s %7s\n' '' "$DIM" gnat "$OFF" \
            "$(bar "$(scaled "$g" "$peak" 30)" 30)" "$g"
    done
}

run_memory(){
    local p c r n=0 total; total=$(count "$PROGRAMS")
    heading "PEAK MEMORY" "resident set at its high point, in megabytes"
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

printf '\n  %sada83%s   %s\n' "$BOLD" "$OFF" "$("$ada83" --version 2>&1 | head -1)"
[ $gnat = 1 ] && printf '  %sgnat%s    %s\n' "$BOLD" "$OFF" "$(gnatmake --version 2>&1 | head -1)"
printf '  %shost%s    %s %s, %s cpus' "$BOLD" "$OFF" "$(uname -s)" "$(uname -m)" \
    "$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo '?')"
[ -n "$(cpu_model)" ] && printf ', %s' "$(cpu_model)"
printf '\n  %smethod%s  median of %s runs after %s warmup, at -O%s\n' "$BOLD" "$OFF" "$REPEATS" "$WARMUP" "$OPT"

case $mode in
    stages)  run_stages ;;
    parser)  run_parser ;;
    corpus)  run_corpus ;;
    compare) run_compare "$reference" ;;
    profile) run_profile ;;
    codegen) run_codegen "$gnat" ;;
    memory)  run_memory ;;
    all)     run_stages; run_parser; run_corpus; run_codegen "$gnat"; run_memory ;;
    *)       show_cursor; echo "bench.sh: unknown mode '$mode'" >&2; usage >&2; exit 2 ;;
esac
show_cursor
echo
