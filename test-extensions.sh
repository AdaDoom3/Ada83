#!/usr/bin/env bash
set -uo pipefail

# Extended tests for the extensions this compiler documents in the readme,
# beyond what ACATS (test.sh) covers:
#
#   1. GNAT-style linker naming — a library subprogram's symbol is prefixed
#      _ada_, so `procedure Main` cannot collide with the C entry point and
#      `procedure Sleep` cannot interpose on libc. pragma Import and pragma
#      Export names are left untouched.
#   2. The generated main captures argc/argv, and the vendor package
#      Extension_Command_Line exposes them.
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

nm_tool=$(command -v nm || command -v llvm-nm || true)

work=$(mktemp -d "${TMPDIR:-/tmp}/ada83-ext.XXXXXX")
[[ "${KEEP_WORK:-0}" = 1 ]] || trap 'rm -rf "$work"' EXIT
cd "$work"

passed=0 failed=0 skipped=0

report(){ # status name detail
    case $1 in
        pass) passed=$((passed+1));  printf '  %sok%s   %s\n' "$GOOD" "$OFF" "$2" ;;
        skip) skipped=$((skipped+1)); printf '  %sskip%s %s — %s\n' "$DIM" "$OFF" "$2" "$3" ;;
        *)    failed=$((failed+1));  printf '  %sFAIL%s %s — %s\n' "$BAD" "$OFF" "$2" "$3" ;;
    esac
}

compile(){ # source... -o exe ; stdout+stderr to compile.log
    "$compiler" "$@" >compile.log 2>&1
}

run_built(){ # exe args... ; stdout to run.out, status in $?
    local exe=./$1; shift
    [[ -x $exe || -x $exe.exe ]] || return 127
    "$exe" "$@" >run.out 2>run.err
}

# ---- 1. linker naming ------------------------------------------------------

printf '\n  %sLibrary subprogram naming%s\n' "$BOLD" "$OFF"

cat > mainname.ada <<'EOF'
with Text_IO;
procedure Main is
begin
  Text_IO.Put_Line ("main procedure ran");
end Main;
EOF
if compile mainname.ada -o mainname && run_built mainname &&
   [[ "$(cat run.out)" == "main procedure ran" ]]
then report pass "procedure Main links and runs beside the C entry point"
else report fail "procedure Main links and runs beside the C entry point" \
                 "$(tail -3 compile.log run.err 2>/dev/null | tr '\n' ' ')"
fi

cat > sleep.ada <<'EOF'
with Text_IO;
procedure Sleep is
begin
  Text_IO.Put_Line ("not libc sleep");
end Sleep;
EOF
if compile sleep.ada -o sleepprog && run_built sleepprog &&
   [[ "$(cat run.out)" == "not libc sleep" ]]
then report pass "procedure Sleep does not interpose on libc"
else report fail "procedure Sleep does not interpose on libc" \
                 "$(tail -3 compile.log run.err 2>/dev/null | tr '\n' ' ')"
fi

if [[ -n $nm_tool ]]; then
    symbols=$("$nm_tool" mainname 2>/dev/null || "$nm_tool" mainname.exe 2>/dev/null)
    if grep -qw '_ada_main' <<<"$symbols"
    then report pass "library subprogram symbol carries the _ada_ prefix"
    else report fail "library subprogram symbol carries the _ada_ prefix" \
                     "no _ada_main among the program's symbols"
    fi
else
    report skip "library subprogram symbol carries the _ada_ prefix" "no nm on PATH"
fi

cat > expo.ada <<'EOF'
with Text_IO;
procedure Expo is
begin
  Text_IO.Put_Line ("exported");
end Expo;
pragma Export (C, Expo, "my_c_entry");
EOF
if compile expo.ada -o expo && run_built expo; then
    if [[ -n $nm_tool ]]; then
        symbols=$("$nm_tool" expo 2>/dev/null || "$nm_tool" expo.exe 2>/dev/null)
        if grep -qw '_ada_expo' <<<"$symbols"
        then report fail "pragma Export keeps its symbol out of the _ada_ namespace" \
                         "_ada_expo emitted despite pragma Export"
        else report pass "pragma Export keeps its symbol out of the _ada_ namespace"
        fi
    else
        report skip "pragma Export keeps its symbol out of the _ada_ namespace" "no nm on PATH"
    fi
else
    report fail "pragma Export keeps its symbol out of the _ada_ namespace" \
                "$(tail -3 compile.log 2>/dev/null | tr '\n' ' ')"
fi

cat > helperlib.ada <<'EOF'
package Helper_Lib is
  function Double (X : Integer) return Integer;
end;
package body Helper_Lib is
  function Double (X : Integer) return Integer is
    begin
      return X * 2;
    end;
end;
EOF
cat > usehelper.ada <<'EOF'
with Text_IO;
with Helper_Lib;
procedure Use_Helper is
  package Int_IO is new Text_IO.Integer_IO (Integer);
begin
  Int_IO.Put (Helper_Lib.Double (21), Width => 1);
  Text_IO.New_Line;
end;
EOF
if compile --ir helperlib.ada -o helperlib.ll &&
   compile usehelper.ada helperlib.ll -o usehelper &&
   run_built usehelper && [[ "$(cat run.out)" == "42" ]]
then report pass "naming stays consistent across separate compilations"
else report fail "naming stays consistent across separate compilations" \
                 "$(tail -3 compile.log run.err 2>/dev/null | tr '\n' ' ')"
fi

# ---- 2. Extension_Command_Line ---------------------------------------------

printf '\n  %sExtension_Command_Line%s\n' "$BOLD" "$OFF"

cat > args.ada <<'EOF'
with Text_IO;
with Extension_Command_Line;
procedure Args is
  package Int_IO is new Text_IO.Integer_IO (Integer);
begin
  Int_IO.Put (Extension_Command_Line.Argument_Count, Width => 1);
  Text_IO.New_Line;
  for I in 1 .. Extension_Command_Line.Argument_Count loop
    Text_IO.Put ("[");
    Text_IO.Put (Extension_Command_Line.Argument (I));
    Text_IO.Put_Line ("]");
  end loop;
end Args;
EOF
if ! compile args.ada -o args; then
    report fail "Extension_Command_Line compiles" \
                "$(tail -3 compile.log | tr '\n' ' ')"
else
    report pass "Extension_Command_Line compiles"

    if run_built args alpha "two words" "" last &&
       [[ "$(cat run.out)" == "$(printf '4\n[alpha]\n[two words]\n[]\n[last]')" ]]
    then report pass "arguments round-trip, including spaces and the empty string"
    else report fail "arguments round-trip, including spaces and the empty string" \
                     "got: $(tr '\n' '|' < run.out)"
    fi

    if run_built args && [[ "$(cat run.out)" == "0" ]]
    then report pass "Argument_Count is 0 with no arguments"
    else report fail "Argument_Count is 0 with no arguments" \
                     "got: $(tr '\n' '|' < run.out)"
    fi

    long=$(printf 'x%.0s' $(seq 1 4000))
    if run_built args "$long" && [[ "$(sed -n '2p' run.out)" == "[$long]" ]]
    then report pass "a 4000-character argument arrives intact"
    else report fail "a 4000-character argument arrives intact" \
                     "length came back $(sed -n '2p' run.out | wc -c)"
    fi
fi

cat > name.ada <<'EOF'
with Text_IO;
with Extension_Command_Line;
procedure Name is
begin
  Text_IO.Put_Line (Extension_Command_Line.Command_Name);
end Name;
EOF
if compile name.ada -o nameprog && run_built nameprog &&
   grep -q 'nameprog' run.out
then report pass "Command_Name names the executable"
else report fail "Command_Name names the executable" \
                 "got: $(cat run.out 2>/dev/null)"
fi

cat > range.ada <<'EOF'
with Text_IO;
with Extension_Command_Line;
procedure Range_Check is
begin
  begin
    Text_IO.Put_Line (Extension_Command_Line.Argument
                        (Extension_Command_Line.Argument_Count + 1));
    Text_IO.Put_Line ("no exception");
  exception
    when Constraint_Error => Text_IO.Put_Line ("constraint_error");
  end;
end Range_Check;
EOF
if compile range.ada -o rangeprog &&
   run_built rangeprog && [[ "$(cat run.out)" == "constraint_error" ]] &&
   run_built rangeprog one two && [[ "$(cat run.out)" == "constraint_error" ]]
then report pass "Argument beyond Argument_Count raises Constraint_Error"
else report fail "Argument beyond Argument_Count raises Constraint_Error" \
                 "got: $(cat run.out 2>/dev/null)"
fi

# ---- summary ---------------------------------------------------------------

total=$((passed + failed + skipped))
printf '\n  %s%d tests: %d passed, %d failed, %d skipped%s\n\n' \
    "$BOLD" "$total" "$passed" "$failed" "$skipped" "$OFF"
[[ $failed = 0 ]]
