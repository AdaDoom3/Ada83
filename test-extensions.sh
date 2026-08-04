#!/usr/bin/env bash
set -uo pipefail

# Runs the extension tests — the .ada programs under extensions/ in
# tests.zip. They cover what ACATS (test.sh) cannot see: the _ada_ symbol
# prefix on library subprograms, and Extension_Command_Line.
#
# Each test is one self-reporting Ada program: it prints PASSED, or one or
# more FAILED lines. Comment headers direct the harness:
#
#   -- ARGS: alpha "two words"     command-line arguments for the run
#   -- LINK: other.ada             unit to compile separately and link in
#   -- SYMBOL: _ada_main           symbol nm must find in the executable
#   -- SYMBOL-NOT: _ada_expo       symbol nm must not find
#
# A file named by some -- LINK: header is a support unit, not a test.
#
# Usage: bash test-extensions.sh
# Environment:
#   ADA83  path to the compiler under test (default: first bin-*/ada83 found)

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)

if [ -t 1 ] && [ "${NO_COLOUR:-0}" != 1 ]
then BOLD=$'\033[1m' DIM=$'\033[2m' GOOD=$'\033[32m' BAD=$'\033[31m' OFF=$'\033[0m'
else BOLD='' DIM='' GOOD='' BAD='' OFF=''
fi

compiler=${ADA83:-}
if [[ -z $compiler ]]; then
    for candidate in "$here"/bin-*/ada83 "$here"/bin-*/ada83.exe; do
        [[ -x $candidate ]] && { compiler=$candidate; break; }
    done
fi
[[ -n $compiler && -x $compiler ]] ||
    { echo "test-extensions.sh: no compiler; build one or set ADA83" >&2; exit 1; }

if [[ ! -d $here/extensions ]]; then
    [[ -f $here/tests.zip ]] ||
        { echo "test-extensions.sh: no extensions/ directory and no tests.zip" >&2; exit 1; }
    (cd "$here" && { unzip -qo tests.zip 'extensions/*' 2>/dev/null ||
                     tar -xf tests.zip extensions; }) ||
        { echo "test-extensions.sh: cannot unpack extensions/ from tests.zip" >&2; exit 1; }
fi

nm_tool=$(command -v nm || command -v llvm-nm || true)

work=$(mktemp -d "${TMPDIR:-/tmp}/ada83-ext.XXXXXX")
[[ "${KEEP_WORK:-0}" = 1 ]] || trap 'rm -rf "$work"' EXIT

header(){ # file key -> value of the last "-- KEY: ..." line
    sed -n "s/^-- $2: //p" "$1" | tail -1
}

# support units are those some test's -- LINK: header names
declare -A is_support=()
for source in "$here"/extensions/*.ada; do
    linked=$(header "$source" LINK)
    [[ -n $linked ]] && is_support[$linked]=1
done

passed=0 failed=0 skipped=0

report(){ # status name detail
    case $1 in
        pass) passed=$((passed+1));  printf '  %sok%s   %s\n' "$GOOD" "$OFF" "$2" ;;
        skip) skipped=$((skipped+1)); printf '  %sskip%s %s — %s\n' "$DIM" "$OFF" "$2" "$3" ;;
        *)    failed=$((failed+1));  printf '  %sFAIL%s %s — %s\n' "$BAD" "$OFF" "$2" "$3" ;;
    esac
}

printf '\n  %sExtension tests%s\n' "$BOLD" "$OFF"

for source in "$here"/extensions/*.ada; do
    name=$(basename "$source" .ada)
    [[ -n ${is_support[$name.ada]:-} ]] && continue

    dir=$work/$name
    mkdir -p "$dir"
    fragments=()

    linked=$(header "$source" LINK)
    if [[ -n $linked ]]; then
        if ! "$compiler" --ir "$here/extensions/$linked" -o "$dir/linked.ll" \
             >"$dir/compile.log" 2>&1; then
            report fail "$name" "support unit $linked: $(tail -2 "$dir/compile.log" | tr '\n' ' ')"
            continue
        fi
        fragments+=("$dir/linked.ll")
    fi

    if ! (cd "$dir" && "$compiler" "$source" ${fragments[@]+"${fragments[@]}"} \
          -o "$dir/$name" >>"$dir/compile.log" 2>&1); then
        report fail "$name" "$(tail -2 "$dir/compile.log" | tr '\n' ' ')"
        continue
    fi
    # prefer the real .exe: msys bash resolves the extension-less path for
    # -x and for running, but nm needs the file that actually exists
    exe=$dir/$name
    [[ -f $exe.exe ]] && exe=$exe.exe

    symbol=$(header "$source" SYMBOL)
    symbol_not=$(header "$source" SYMBOL-NOT)
    if [[ -n $symbol || -n $symbol_not ]]; then
        if [[ -z $nm_tool ]]; then
            report skip "$name" "symbol check needs nm, which is not on PATH"
            continue
        fi
        # Mach-O prepends an underscore to every C-level symbol, so accept
        # the name with or without one leading underscore
        symbols=$("$nm_tool" "$exe" 2>/dev/null)
        if [[ -n $symbol ]] &&
           ! grep -qE "[[:space:]]_?$symbol\$" <<<"$symbols"; then
            report fail "$name" "symbol $symbol missing from the executable"
            continue
        fi
        if [[ -n $symbol_not ]] &&
           grep -qE "[[:space:]]_?$symbol_not\$" <<<"$symbols"; then
            report fail "$name" "symbol $symbol_not present in the executable"
            continue
        fi
    fi

    args=$(header "$source" ARGS)
    eval "set -- $args"
    if ! "$exe" "$@" >"$dir/run.out" 2>"$dir/run.err"; then
        report fail "$name" "exited $? — $(tail -2 "$dir/run.err" | tr '\n' ' ')"
        continue
    fi
    if grep -q FAILED "$dir/run.out"; then
        report fail "$name" "$(grep FAILED "$dir/run.out" | head -1)"
    elif grep -q PASSED "$dir/run.out"; then
        report pass "$name"
    else
        report fail "$name" "printed neither PASSED nor FAILED: $(head -1 "$dir/run.out")"
    fi
done

total=$((passed + failed + skipped))
printf '\n  %s%d tests: %d passed, %d failed, %d skipped%s\n\n' \
    "$BOLD" "$total" "$passed" "$failed" "$skipped" "$OFF"
[[ $failed = 0 ]]
