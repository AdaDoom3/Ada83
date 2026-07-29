#!/usr/bin/env bash
#
#  Usage:
#    bench/bench.sh            
#    bench/bench.sh stages
#    bench/bench.sh compile
#    bench/bench.sh runtime
#    REPEATS=7 bench/bench.sh runtime
#    WIDE_SIZE=800 bench/bench.sh compile
#
#  Environment:
#    OPT_PIPELINE  none (default) or llvm
#    REPEATS     timed repetitions, best-of is reported (default 5)
#    WIDE_SIZE   subprograms in the generated compile-stress unit (400)
#    GNAT_BIN    override the GNAT bin directory
#    OPT         optimisation level for both back ends (default 2)

set -uo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
root=$(cd -- "$here/.." && pwd)
programs="$here/programs"
stamp=$(date +%Y%m%d-%H%M%S)
out="$here/results/$stamp"
work="$out/work"
src_dir="$work/src"
a83_dir="$work/ada83"
gnat_dir="$work/gnat"
log_dir="$out/logs"
mkdir -p "$src_dir" "$a83_dir" "$gnat_dir" "$log_dir"

KEEP_RUNS=${KEEP_RUNS:-3}
KEEP_WORK=${KEEP_WORK:-1}
ls -1dt "$here"/results/*/ 2>/dev/null | tail -n +$((KEEP_RUNS + 1)) \
  | while read -r old; do rm -rf "$old"; done

REPEATS=${REPEATS:-5}
WIDE_SIZE=${WIDE_SIZE:-400}
OPT=${OPT:-2}
mode=${1:-all}

ada83="$root/ada83"
rts="$root/rts"

#  ---------------------------------------------------------------- GNAT

find_gnat () {
  if [ -n "${GNAT_BIN:-}" ]; then echo "$GNAT_BIN"; return; fi
  local cached
  #  A GNAT install has several bin directories; only one holds the
  #  drivers, so ask for the driver rather than for the directory.
  cached=$(find "$here/toolchain" -type f -name gnatmake -path '*gnat_native*' \
             2>/dev/null | head -1)
  if [ -n "$cached" ]; then dirname "$cached"; return; fi
  command -v gnatmake >/dev/null 2>&1 && dirname "$(command -v gnatmake)"
}

gnat_bin=$(find_gnat)
if [ -z "$gnat_bin" ] || [ ! -x "$gnat_bin/gnatmake" ]; then
  echo "GNAT not found.  Install it with Alire:" >&2
  echo "  cd $here/toolchain && ALIRE_SETTINGS_DIR=\$PWD/alire-settings \\" >&2
  echo "      ./alr -n toolchain --select gnat_native gprbuild" >&2
  echo "or point GNAT_BIN at an existing installation." >&2
  exit 1
fi

[ -x "$ada83" ] || { echo "building ada83"; make -C "$root" >/dev/null || exit 1; }

gnat_version=$("$gnat_bin/gnatmake" --version 2>/dev/null | head -1)

#  This Alire toolchain ships libgnat.a but no runtime .ali files, and
#  its bundled gcc cannot find the system C startup objects.  Without
#  both of the following GNAT compiles and never links, so only the
#  front-end column of this report would be real -- which is how it
#  read until now.
#
#    -a           lets gnatmake recompile library units from the
#                 adainclude sources, which is where the missing .ali
#                 files come from.
#    -largs -B    points the link at the system crtbegin.o.
#
#  Both are properties of this installation, not of GNAT, so they are
#  discovered rather than assumed: if the .ali files are present the -a
#  costs nothing, and if crtbegin.o is found the -B is redundant.
gnat_adainclude=$(find "$gnat_bin/.." -type d -name adainclude 2>/dev/null | head -1)
gnat_rts_flags=()
[ -n "$gnat_adainclude" ] && [ -z "$(ls "$gnat_adainclude/../adalib"/*.ali 2>/dev/null)" ] \
  && gnat_rts_flags=(-a -I"$gnat_adainclude")
gnat_crt=$(ls -d /usr/lib/gcc/*/*/ 2>/dev/null | while read -r d; do
             [ -f "$d/crtbegin.o" ] && echo "$d" && break; done)
gnat_link_flags=()
[ -n "$gnat_crt" ] && gnat_link_flags=(-largs "-B$gnat_crt")
ada83_commit=$(cd "$root" && git rev-parse --short HEAD 2>/dev/null || echo unknown)

#  ------------------------------------------------------------ utilities

#  Best-of-REPEATS wall time in milliseconds.  Best rather than mean:
#  the machine is shared with whatever else is running, so the minimum
#  is the measurement least polluted by that.
time_best () {
  local best="" t start end
  for _ in $(seq "$REPEATS"); do
    start=$(date +%s%N)
    "$@" >/dev/null 2>&1
    end=$(date +%s%N)
    t=$(( (end - start) / 1000000 ))
    if [ -z "$best" ] || [ "$t" -lt "$best" ]; then best=$t; fi
  done
  echo "$best"
}

rule () { printf '%s\n' "-------------------------------------------------------------------"; }

#  Every build step logs to its own file; a failure names the log and
#  quotes its first line, so the table says WHY rather than just that.
last_failure=""

run_step () {
  local log=$1; shift
  if "$@" >"$log" 2>&1; then return 0; fi
  last_failure="$log"
  return 1
}

failure_reason () {
  [ -n "$last_failure" ] && [ -s "$last_failure" ] || { echo "(no output)"; return; }
  grep -m1 -E "error|Error|ERROR|undefined|cannot|fatal" "$last_failure" \
    | cut -c1-58 || head -1 "$last_failure" | cut -c1-58
}

#  ada83 source -> .ll only.
ada83_frontend () {
  run_step "$log_dir/$(basename "$1" .adb).a83-front.log" \
    "$ada83" "$1" -o "$2" -I "$rts"
}

#  GNAT ships a compiled runtime; ada83's runtime is source that any
#  build would otherwise recompile.  Compiling it once here is what
#  makes the two full-pipeline numbers comparable — otherwise ada83 is
#  charged for a runtime build GNAT did at release time.
prepare_runtime () {
  runtime_ll=()
  #  bench_support is a package that WITHs TEXT_IO, so compiling it
  #  emits TEXT_IO's bodies with the same symbol numbering a client
  #  gets — the role report.ll plays for the conformance suite.
  #  Compiling text_io.adb directly does NOT work here: the numbering
  #  of an overloaded subprogram's symbol depends on the compilation
  #  context, so a standalone body defines names no client refers to.
  [ -f "$a83_dir/bench_support.ll" ] \
    || run_step "$log_dir/bench_support.log" \
         "$ada83" "$here/bench_support.adb" -o "$a83_dir/bench_support.ll" \
           -I "$rts" -I "$here"
  [ -f "$a83_dir/bench_support.ll" ] && runtime_ll+=("$a83_dir/bench_support.ll")
  #  SYSTEM is elaborated by every program but WITHed by none of these,
  #  so its elaboration body has to be linked in explicitly.
  [ -f "$a83_dir/system.ll" ] \
    || run_step "$log_dir/system.log" \
         "$ada83" "$rts/system.ads" -o "$a83_dir/system.ll" -I "$rts"
  [ -f "$a83_dir/system.ll" ] && runtime_ll+=("$a83_dir/system.ll")
  local extra
  for extra in "$rts"/rt.ll "$rts"/rt_wrappers.ll; do
    [ -f "$extra" ] && runtime_ll+=("$extra")
  done
}
runtime_ll=()

#  ada83 source -> executable, through its own route.
ada83_full () {
  local src=$1 exe=$2 base
  base=$(basename "$src" .adb)
  run_step "$log_dir/$base.a83-compile.log" \
    "$ada83" "$src" -o "$a83_dir/$base.ll" -I "$rts" || return 1
  run_step "$log_dir/$base.a83-link.log" \
    llvm-link -o "$a83_dir/$base.bc" "$a83_dir/$base.ll" "${runtime_ll[@]}" \
    || return 1
  #  LLVM's middle end, when asked for.  Neither shipped pipeline runs
  #  it -- this harness went straight to llc and run_acats.sh uses lli
  #  -- so mem2reg, SROA and inlining never see our IR, and the emitted
  #  IR is very nearly the machine code.  That is a defensible choice
  #  (it makes the front end's own output quality the thing that
  #  matters) but it was never a decided one, so the harness now
  #  measures both and names which is which.
  #
  #    OPT_PIPELINE=none  emit -> llc -> clang        (as shipped)
  #    OPT_PIPELINE=llvm  emit -> opt -O2 -> llc -> clang
  local stage_bc="$a83_dir/$base.bc"
  if [ "${OPT_PIPELINE:-none}" = llvm ]; then
    run_step "$log_dir/$base.a83-opt.log" \
      opt "-O$OPT" -o "$a83_dir/$base.opt.bc" "$a83_dir/$base.bc" || return 1
    stage_bc="$a83_dir/$base.opt.bc"
  fi
  #  PIC, because the toolchain links position-independent executables
  #  by default and the default relocation model does not.
  run_step "$log_dir/$base.a83-llc.log" \
    llc "-O$OPT" --relocation-model=pic -filetype=obj \
        -o "$a83_dir/$base.o" "$stage_bc" || return 1
  run_step "$log_dir/$base.a83-ld.log" \
    clang "-O$OPT" -o "$exe" "$a83_dir/$base.o" -lm -lpthread || return 1
}

gnat_frontend () {
  run_step "$log_dir/$(basename "$1" .adb).gnat-front.log" \
    "$gnat_bin/gcc" -c -S "-O$OPT" "$1" -o "$2"
}

#  Cold: -f forces a rebuild, so the number is the work itself.
gnat_full () {
  local base=${1%.adb}
  run_step "$log_dir/$base.gnat-cold.log" \
    "$gnat_bin/gnatmake" -f "-O$OPT" ${gnat_rts_flags[@]+"${gnat_rts_flags[@]}"} \
      "$src_dir/$1" -o "$gnat_dir/$2" \
      -D "$gnat_dir" -aO"$gnat_dir" ${gnat_link_flags[@]+"${gnat_link_flags[@]}"}
}

#  Warm: no -f, so gnatmake consults the ALI files and rebuilds only
#  what changed.  With nothing changed this is the cost of deciding
#  there is nothing to do.
gnat_full_warm () {
  local base=${1%.adb}
  run_step "$log_dir/$base.gnat-warm.log" \
    "$gnat_bin/gnatmake" "-O$OPT" ${gnat_rts_flags[@]+"${gnat_rts_flags[@]}"} \
      "$src_dir/$1" -o "$gnat_dir/$2" \
      -D "$gnat_dir" -aO"$gnat_dir" ${gnat_link_flags[@]+"${gnat_link_flags[@]}"}
}

#  ada83's warm path is the same pipeline: it writes ALI files with
#  checksums for the units it loads, but it always recompiles the unit
#  it was handed.  Measuring it against gnatmake's warm path is how the
#  difference becomes visible rather than assumed.
ada83_full_warm () { ada83_full "$1" "$2"; }

#  --------------------------------------------------------------- header

{
echo "==================================================================="
echo " ada83 vs GNAT — compiler and generated-code benchmark"
echo "==================================================================="
echo " date            $(date -u '+%Y-%m-%d %H:%M:%S UTC')"
echo " ada83           $ada83_commit  ($(wc -l < "$root/ada83.c") lines)"
echo " GNAT            $gnat_version"
echo " host            $(nproc) cores, $(uname -m)"
echo " repetitions     $REPEATS (best-of reported)"
echo " back-end opt    -O$OPT"
echo " results         $out"
echo
echo " Front end means source to the compiler's own output: .ll for"
echo " ada83, .s for GNAT.  Full means source to a linked executable"
echo " through each compiler's own route, which for ada83 includes llc"
echo " and clang — a code generator it does not own."
} | tee "$out/report.txt"

#  --------------------------------------------------------------- stages

run_stages () {
  {
  echo; rule; echo " STAGES — where each compiler spends its own time"; rule
  } | tee -a "$out/report.txt"

  #  ada83: a -pg build, attributed by symbol.  The §-sections of
  #  ada83.c are recognisable from the names, so the flat profile maps
  #  onto them without needing the compiler to instrument itself.
  if [ ! -x "$work/ada83-pg" ]; then
    echo " building instrumented ada83 (-pg)…" | tee -a "$out/report.txt"
    gcc -O2 -pg -w -o "$work/ada83-pg" "$root/ada83.c" -lm -lpthread || {
      echo " could not build -pg binary; skipping ada83 stage profile" \
        | tee -a "$out/report.txt"; return; }
  fi

  "$here/gen_wide.sh" "$WIDE_SIZE" > "$src_dir/wide.adb"

  ( cd "$work" && ./ada83-pg "$src_dir/wide.adb" -o wide-pg.ll -I "$rts" >/dev/null 2>&1 )
  if [ ! -f "$work/gmon.out" ]; then
    echo " no gmon.out produced; skipping ada83 stage profile" \
      | tee -a "$out/report.txt"
  else
    gprof -b -p "$work/ada83-pg" "$work/gmon.out" > "$out/ada83-gprof.txt" 2>/dev/null
    {
    echo
    echo " ada83 — self time by stage (gprof flat profile, attributed by symbol)"
    echo
    #  gprof's flat profile puts self seconds in column 3 and the symbol
    #  last on the line; a row for a symbol with no call count is
    #  shorter than the rest, which is why the name is taken from $NF
    #  rather than from a fixed column.  The buckets are listed and
    #  sorted by hand because array sorting is a GNU awk extension and
    #  this has to run wherever the benchmark does.
    awk '
      /^ *[0-9]/ {
        seconds = $3 + 0; name = $NF
        total += seconds
        if (name ~ /^(Scan|Lex|Keyword|Token_Text|Next_Token)/)           b = "Lexer (§7)"
        else if (name ~ /^(Parse|Node_New|Node_List|Syntax)/)             b = "Parser (§9)"
        else if (name ~ /^(Resolve|Analyze|Select_|Interp|Overload)/)     b = "Name and overload resolution (§11-12)"
        else if (name ~ /^(Type_|Constrain|Freeze|Component_|Discrimin)/) b = "Types (§10)"
        else if (name ~ /^(Symbol|Scope)/)                                b = "Symbol table (§11)"
        else if (name ~ /^(Check_|Fold|Eval|Const_|Static_)/)             b = "Checking and folding (§12)"
        else if (name ~ /^(Generate|Emit|LLVM_|Val_)/)                    b = "Code generation (§13)"
        else if (name ~ /^(Elab|Library|Catalog|Load_|Compile_)/)         b = "Library and elaboration (§14-15,17)"
        else if (name ~ /^(Arena|String_|Slice|Hash|Big|U128|I128|Simd)/) b = "Foundation (§1-6,18)"
        else                                                              b = "Other"
        if (!(b in s)) { order[++n] = b }
        s[b] += seconds
      }
      END {
        if (total <= 0) { print "  (no samples — program too fast to profile)"; exit }
        for (i = 1; i <= n; i++)
          for (j = i + 1; j <= n; j++)
            if (s[order[j]] > s[order[i]]) { t = order[i]; order[i] = order[j]; order[j] = t }
        for (i = 1; i <= n; i++)
          printf "   %-40s %8.2fs  %5.1f%%\n", order[i], s[order[i]],
                 100 * s[order[i]] / total
        printf "   %-40s %8.2fs\n", "TOTAL", total
      }' "$out/ada83-gprof.txt"

    #  The bucket table says which subsystem to look at; this says which
    #  function to open.  A stage profile without it sends the reader
    #  back to the raw gprof output every time.
    echo
    echo " ada83 — the twelve costliest symbols"
    echo
    awk '/^ *[0-9]/ && shown < 12 {
           printf "   %8.2fs  %12s calls  %s\n", $3, ($4 == "" ? "-" : $4), $NF
           shown++
         }' "$out/ada83-gprof.txt"
    } | tee -a "$out/report.txt"
    rm -f "$work/gmon.out"
  fi

  #  GNAT reports its own phases.
  {
  echo
  echo " GNAT — self time by phase (gnat1 -ftime-report)"
  echo
  } | tee -a "$out/report.txt"
  ( cd "$gnat_dir" && "$gnat_bin/gcc" -c "-O$OPT" -ftime-report \
      "$src_dir/wide.adb" -o wide-gnat.o ) > "$out/gnat-time-report.txt" 2>&1
  grep -E '^ (phase|TOTAL)' "$out/gnat-time-report.txt" \
    | sed 's/^/  /' | tee -a "$out/report.txt" \
    || echo "  (no phase report)" | tee -a "$out/report.txt"

  echo | tee -a "$out/report.txt"
  echo " Note: the two profiles are built differently — ada83's is a" \
    | tee -a "$out/report.txt"
  echo " sampled flat profile of a -pg build, GNAT's is its own" \
    | tee -a "$out/report.txt"
  echo " instrumented phase accounting.  Compare shapes, not totals." \
    | tee -a "$out/report.txt"
}

#  -------------------------------------------------------------- compile

run_compile () {
  {
  echo; rule; echo " COMPILE TIME"; rule
  echo
  echo "  cold = a full rebuild.  warm = the same command run again with"
  echo "  nothing changed, which is what an edit-rebuild cycle actually"
  echo "  costs."
  echo
  printf "  %-10s %9s %9s %7s  %9s %9s %7s  %9s %9s %7s\n" \
    "program" "a83 front" "gnat frnt" "ratio" \
    "a83 cold" "gnat cold" "ratio" "a83 warm" "gnat warm" "ratio"
  } | tee -a "$out/report.txt"

  "$here/gen_wide.sh" "$WIDE_SIZE" > "$src_dir/wide.adb"
  prepare_runtime

  local srcs=() f
  for f in "$programs"/*.adb; do srcs+=("$f"); done
  srcs+=("$src_dir/wide.adb")

  for f in "${srcs[@]}"; do
    local base af gf ax gx aw gw ratio_f ratio_x ratio_w
    base=$(basename "$f" .adb)
    [ "$f" -ef "$src_dir/$base.adb" ] || cp -f "$f" "$src_dir/$base.adb"

    #  One untimed build of each first: a step that fails should be
    #  reported as a failure, not timed as if it had done the work.
    if ! ada83_full "$src_dir/$base.adb" "$a83_dir/$base.a83"; then
      printf "  %-10s %s\n" "$base" "ada83 BUILD FAIL: $(failure_reason)" \
        | tee -a "$out/report.txt"; continue
    fi
    if ! gnat_full "$base.adb" "$base.gnat"; then
      printf "  %-10s %s\n" "$base" "GNAT BUILD FAIL: $(failure_reason)" \
        | tee -a "$out/report.txt"; continue
    fi

    af=$(time_best ada83_frontend "$src_dir/$base.adb" "$a83_dir/$base.ll")
    gf=$(time_best gnat_frontend  "$src_dir/$base.adb" "$gnat_dir/$base.s")
    ax=$(time_best ada83_full     "$src_dir/$base.adb" "$a83_dir/$base.a83")
    gx=$(time_best gnat_full      "$base.adb"          "$base.gnat")
    #  Both warm paths run against the artefacts the cold runs left.
    aw=$(time_best ada83_full_warm "$src_dir/$base.adb" "$a83_dir/$base.a83")
    gw=$(time_best gnat_full_warm  "$base.adb"          "$base.gnat")

    ratio_f=$(awk -v a="$af" -v g="$gf" 'BEGIN{ if (g>0) printf "%.2fx", a/g; else print "-" }')
    ratio_x=$(awk -v a="$ax" -v g="$gx" 'BEGIN{ if (g>0) printf "%.2fx", a/g; else print "-" }')
    ratio_w=$(awk -v a="$aw" -v g="$gw" 'BEGIN{ if (g>0) printf "%.2fx", a/g; else print "-" }')
    printf "  %-10s %7sms %7sms %7s  %7sms %7sms %7s  %7sms %7sms %7s\n" \
      "$base" "$af" "$gf" "$ratio_f" "$ax" "$gx" "$ratio_x" \
      "$aw" "$gw" "$ratio_w" | tee -a "$out/report.txt"
  done

  {
  echo
  echo "  ratio below 1.00x means ada83 is faster."
  echo
  echo "  A warm number far below its cold number means the compiler"
  echo "  skipped work it had already done.  ada83 writes ALI files with"
  echo "  checksums for the units it loads, but always recompiles the"
  echo "  unit it was handed, so its warm and cold numbers should track"
  echo "  each other; gnatmake's should not.  That gap is a result, not"
  echo "  a flaw in the measurement."
  } | tee -a "$out/report.txt"
}

#  -------------------------------------------------------------- runtime

run_runtime () {
  {
  echo; rule; echo " RUNTIME — speed of the generated code"; rule
  echo
  printf "  %-14s %12s %12s %9s  %s\n" \
    "program" "ada83" "GNAT" "ratio" "agree?"
  } | tee -a "$out/report.txt"

  prepare_runtime

  local f
  for f in "$programs"/*.adb; do
    local base at gt ratio a_out g_out agree
    base=$(basename "$f" .adb)
    [ "$f" -ef "$src_dir/$base.adb" ] || cp -f "$f" "$src_dir/$base.adb"

    if ! ada83_full "$src_dir/$base.adb" "$a83_dir/$base.a83"; then
      printf "  %-14s %s\n" "$base" "ada83 BUILD FAIL: $(failure_reason)" \
        | tee -a "$out/report.txt"
      continue
    fi
    if ! gnat_full "$base.adb" "$base.gnat"; then
      printf "  %-14s %s\n" "$base" "GNAT BUILD FAIL: $(failure_reason)" \
        | tee -a "$out/report.txt"
      continue
    fi

    a_out=$("$a83_dir/$base.a83" 2>&1 | tr -d ' \n')
    g_out=$("$gnat_dir/$base.gnat" 2>&1 | tr -d ' \n')
    if [ "$a_out" = "$g_out" ]; then agree="yes"; else agree="NO ($a_out vs $g_out)"; fi

    at=$(time_best "$a83_dir/$base.a83")
    gt=$(time_best "$gnat_dir/$base.gnat")
    ratio=$(awk -v a="$at" -v g="$gt" 'BEGIN{ if (g>0) printf "%.2fx", a/g; else print "-" }')
    printf "  %-14s %10sms %10sms %9s  %s\n" \
      "$base" "$at" "$gt" "$ratio" "$agree" | tee -a "$out/report.txt"
  done

  echo | tee -a "$out/report.txt"
  echo "  ratio below 1.00x means ada83's code is faster." \
    | tee -a "$out/report.txt"
  echo "  'agree' compares the two programs' output — a benchmark whose" \
    | tee -a "$out/report.txt"
  echo "  two builds disagree is measuring nothing." | tee -a "$out/report.txt"
}

case "$mode" in
  stages)  run_stages ;;
  compile) run_compile ;;
  runtime) run_runtime ;;
  all)     run_stages; run_compile; run_runtime ;;
  *) echo "usage: bench.sh [stages|compile|runtime|all]" >&2; exit 2 ;;
esac

#  ------------------------------------------------------------ verdict

{
echo
echo
echo " results   $out"
echo " compare   diff <(grep -E '^  [a-z]' PREVIOUS/report.txt) \\"
echo "                <(grep -E '^  [a-z]' $out/report.txt)"
} | tee -a "$out/report.txt"

#  Reports are small and worth keeping; work trees are large and are
#  not.  Benchmarking must never be the reason the disk fills.
[ "$KEEP_WORK" = "0" ] && rm -rf "$work"
