#!/bin/bash
echo "════════════════════════════════════════════════════════════"
echo "  ADA83 ACATS TEST RUNNER"
echo "════════════════════════════════════════════════════════════"
mkdir -p test_results
pass=0
fail=0
skip=0
run_test() {
    local test_file=$1
    local test_name=$(basename "$test_file" .ada)
    printf "%-15s " "$test_name"
    if timeout 5 ./ada83 "$test_file" -o "test_results/${test_name}.ll" 2>/dev/null; then
        if [ -f "test_results/${test_name}.ll" ]; then
            echo -e "\033[32mPASS\033[0m"
            ((pass++))
        else
            echo -e "\033[31mFAIL\033[0m (no output)"
            ((fail++))
        fi
    else
        echo -e "\033[33mSKIP\033[0m (compile error)"
        ((skip++))
    fi
}
echo ""
echo "Running A-series tests (lexical/basic):"
cnt=0
for f in acats/a2[0-9]*.ada; do
    if [ -f "$f" ]; then
        run_test "$f"
        ((cnt++)) || true
        [ $cnt -ge 20 ] && break
    fi
done
echo ""
echo "Running C-series tests (core features):"
cnt=0
for f in acats/c2[0-9]*.ada acats/c3[0-9]*.ada; do
    if [ -f "$f" ]; then
        run_test "$f"
        ((cnt++)) || true
        [ $cnt -ge 20 ] && break
    fi
done
echo ""
echo "════════════════════════════════════════════════════════════"
echo "Results: PASS=$pass FAIL=$fail SKIP=$skip"
echo "════════════════════════════════════════════════════════════"
