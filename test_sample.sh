#!/bin/bash
# Test a small sample to verify the test harness works

for f in acats/c{51002a,36205a,54a24a}.ada; do
    echo "Testing $f..."
    base=$(basename "$f" .ada)
    
    # Compile
    if ./ada83 "$f" > "/tmp/${base}.ll" 2>/tmp/${base}.err; then
        echo "  ✓ Compiled"
        # Run
        if timeout 2 lli "/tmp/${base}.ll" >/tmp/${base}.out 2>&1; then
            echo "  ✓ Executed (exit 0)"
            if grep -q "PASS\|FAIL" /tmp/${base}.out 2>/dev/null; then
                grep "PASS\|FAIL" /tmp/${base}.out | head -1
            fi
        else
            echo "  ✗ Runtime error"
        fi
    else
        echo "  ✗ Compile failed"
    fi
    echo ""
done
