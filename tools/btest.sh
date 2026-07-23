#!/usr/bin/env bash
# btest.sh — show one B test's expected ERROR lines vs actual diagnostics.
# Usage: tools/btest.sh b45102a [-q]
n=$1
f=acats/$n.ada
[[ -f $f ]] || { echo "no such test $f"; exit 1; }
./ada83 "$f" >/dev/null 2>/tmp/btest.err; rc=$?
echo "=== $n (compiler exit $rc; 0 means WRONG_ACCEPT) ==="
awk '/-- ERROR/{printf "EXPECT %4d: %s\n", NR, $0}' "$f" | cut -c1-100
echo "--- actual diagnostics ---"
grep "^[^:]*:[0-9]" /tmp/btest.err | cut -c1-100
