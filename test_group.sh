#!/bin/bash
GROUP=$1
echo "Testing group ${GROUP}..."
PASS=0
FAIL=0
for test in acats/${GROUP}*.ada; do
    testname=$(basename "$test" .ada)
    ./ada83 "$test" -o "test_results/${testname}.ll" > "acats_logs/${testname}.compile.log" 2>&1
    if [ $? -eq 0 ]; then
        ((PASS++))
    else
        ((FAIL++))
    fi
done
echo "Group ${GROUP}: ${PASS} passed, ${FAIL} failed"
