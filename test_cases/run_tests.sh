#!/bin/bash
# Generic package test harness

cd "$(dirname "$0")/.." || exit 1

echo "========================================="
echo "Generic Package Test Suite"
echo "========================================="
echo

passed=0
failed=0

run_test() {
    local num=$1
    local name=$2
    local file=$3
    local expect_compile=$4
    local expect_elab=$5

    printf "T%d %-30s " "$num" "$name"

    if ./ada83 "$file" > "test_cases/t${num}.ll" 2>&1; then
        if [ "$expect_compile" = "pass" ]; then
            # Check for elaboration code
            if grep -q "__elab" "test_cases/t${num}.ll" 2>/dev/null; then
                has_elab="yes"
            else
                has_elab="no"
            fi

            if [ "$expect_elab" = "$has_elab" ]; then
                echo "✓ PASS"
                ((passed++))
            else
                echo "✗ FAIL (elab: expected=$expect_elab got=$has_elab)"
                ((failed++))
            fi
        else
            echo "✗ FAIL (compiled when should fail)"
            ((failed++))
        fi
    else
        if [ "$expect_compile" = "fail" ]; then
            echo "✓ PASS (correctly rejected)"
            ((passed++))
        else
            error=$(head -1 "test_cases/t${num}.ll" 2>/dev/null)
            echo "✗ FAIL (compile error: $error)"
            ((failed++))
        fi
    fi
}

# Run tests
run_test 1 "Spec only" "test_cases/t1_spec_only.ada" "pass" "no"
run_test 2 "Body no inst" "test_cases/t2_body_no_inst.ada" "pass" "no"
run_test 3 "Instantiation" "test_cases/t3_instantiation.ada" "pass" "yes"
run_test 4 "Multiple generic" "test_cases/t4_multiple_generic.ada" "pass" "no"
run_test 5 "With formal param" "test_cases/t5_with_formal.ada" "pass" "yes"

echo
echo "========================================="
echo "Results: $passed passed, $failed failed"
echo "========================================="

exit $failed
