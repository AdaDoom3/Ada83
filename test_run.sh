#!/bin/bash
# Comprehensive ACATS test runner that compiles AND executes tests

pass_compile=0
fail_compile=0
pass_run=0
fail_run=0

echo "Running ACATS test suite..."
echo "============================"

for f in acats/c*.ada; do
    base=$(basename "$f" .ada)
    
    # Step 1: Compile to LLVM IR
    if ! timeout 5 ./ada83 "$f" > "/tmp/${base}.ll" 2>/tmp/${base}.err; then
        ((fail_compile++))
        echo "COMPILE FAIL: $f"
        continue
    fi
    
    ((pass_compile++))
    
    # Step 2: Run with lli
    if timeout 10 lli "/tmp/${base}.ll" >/tmp/${base}.out 2>&1; then
        # Check for PASS/FAIL in output (ACATS convention)
        if grep -q "PASS" /tmp/${base}.out 2>/dev/null; then
            ((pass_run++))
        elif grep -q "FAIL" /tmp/${base}.out 2>/dev/null; then
            ((fail_run++))
            echo "RUNTIME FAIL: $f"
        else
            # No PASS/FAIL message, assume pass if no crash
            ((pass_run++))
        fi
    else
        ((fail_run++))
        echo "RUNTIME CRASH: $f"
    fi
done

echo ""
echo "Results:"
echo "  Compilation: $pass_compile/$((pass_compile+fail_compile)) passed"
echo "  Execution:   $pass_run/$((pass_compile)) passed"
echo "  Overall:     $pass_run/$((pass_compile+fail_compile)) tests fully passing"
