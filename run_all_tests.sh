#!/bin/bash
# ============================================================================
# COMPREHENSIVE TEST SUITE FOR ADA83 ARM MICROKERNEL
# ============================================================================
# Runs all validation tests for both base and SMP kernels
# ============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test results tracking
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_TOTAL=0

# ============================================================================
# Helper functions
# ============================================================================

print_header() {
    echo -e "\n${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║ $1${NC}"
    echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}\n"
}

print_test() {
    echo -e "${YELLOW}[TEST $TESTS_TOTAL] $1${NC}"
}

pass_test() {
    echo -e "${GREEN}✓ PASS${NC}: $1"
    ((TESTS_PASSED++))
}

fail_test() {
    echo -e "${RED}✗ FAIL${NC}: $1"
    ((TESTS_FAILED++))
}

run_test() {
    local test_name="$1"
    local test_cmd="$2"

    ((TESTS_TOTAL++))
    print_test "$test_name"

    if eval "$test_cmd" > /dev/null 2>&1; then
        pass_test "$test_name"
        return 0
    else
        fail_test "$test_name"
        return 1
    fi
}

# ============================================================================
# BASE KERNEL TESTS
# ============================================================================

test_base_kernel() {
    print_header "BASE KERNEL VALIDATION"

    # Test 1: Validate kernel structure
    run_test "Base kernel structure validation" \
        "./validate_kernel.sh | grep -q '40/40 checks passed'"

    # Test 2: Static analysis
    run_test "Base kernel static analysis" \
        "./analyze_kernel.sh | grep -q 'PASS'"

    # Test 3: Functional simulator
    if [ -f kernel_simulator ]; then
        run_test "Base kernel functional simulation" \
            "./kernel_simulator | grep -q 'ALL TESTS PASSED'"
    else
        print_test "Base kernel functional simulation"
        echo -e "${YELLOW}⊘ SKIP${NC}: Simulator not built"
    fi
}

# ============================================================================
# SMP KERNEL TESTS
# ============================================================================

test_smp_kernel() {
    print_header "SMP KERNEL VALIDATION"

    # Test 4: Build SMP simulator
    run_test "SMP simulator compilation" \
        "gcc -O2 -Wall -Wextra -Wno-unused-parameter -o kernel_smp_simulator kernel_smp_simulator.c -lpthread -lm"

    # Test 5: Run SMP simulator
    run_test "SMP functional simulation (all 6 tests)" \
        "./kernel_smp_simulator | grep -q 'ALL TESTS PASSED'"

    # Test 6: SMP benchmarks compilation
    run_test "SMP benchmarks compilation" \
        "gcc -O2 -Wall -Wextra -o benchmarks_smp benchmarks_smp.c -lpthread -lm"

    # Test 7: Run SMP benchmarks
    run_test "SMP performance benchmarks" \
        "./benchmarks_smp | grep -q 'BENCHMARKS COMPLETE'"
}

# ============================================================================
# CODE QUALITY TESTS
# ============================================================================

test_code_quality() {
    print_header "CODE QUALITY CHECKS"

    # Test 8: Check for Assembly files
    run_test "ARM assembly files present" \
        "test -f boot.s -a -f boot_smp.s"

    # Test 9: Check for Ada files
    run_test "Ada kernel files present" \
        "test -f kernel.adb -a -f kernel_smp.adb"

    # Test 10: Check for linker scripts
    run_test "Linker scripts present" \
        "test -f kernel.ld -a -f kernel_smp.ld"

    # Test 11: Check for Makefiles
    run_test "Build systems present" \
        "test -f Makefile.kernel -a -f Makefile.smp"

    # Test 12: Check line counts (code golf metric)
    total_lines=$(wc -l kernel.adb boot.s kernel_smp.adb boot_smp.s | tail -1 | awk '{print $1}')
    if [ "$total_lines" -lt 3000 ]; then
        ((TESTS_TOTAL++))
        pass_test "Code size under budget ($total_lines lines < 3000)"
    else
        ((TESTS_TOTAL++))
        fail_test "Code size over budget ($total_lines lines >= 3000)"
    fi
}

# ============================================================================
# DOCUMENTATION TESTS
# ============================================================================

test_documentation() {
    print_header "DOCUMENTATION VALIDATION"

    # Test 13: README files
    run_test "Core documentation present" \
        "test -f QUICKSTART.md -o -f MICROKERNEL.md"

    # Test 14: Test documentation
    run_test "Test documentation present" \
        "test -f TESTING_AND_TUNING.md"

    # Test 15: SMP research documentation
    run_test "SMP research documentation present" \
        "test -f SMP_RESEARCH_DOCUMENTATION.md"

    # Test 16: Documentation completeness
    doc_size=$(wc -c *.md 2>/dev/null | tail -1 | awk '{print $1}' || echo 0)
    if [ "$doc_size" -gt 50000 ]; then
        ((TESTS_TOTAL++))
        pass_test "Documentation comprehensive (>50KB)"
    else
        ((TESTS_TOTAL++))
        fail_test "Documentation insufficient (<50KB)"
    fi
}

# ============================================================================
# INTEGRATION TESTS
# ============================================================================

test_integration() {
    print_header "INTEGRATION TESTS"

    # Test 17: Makefile syntax (base)
    run_test "Base Makefile syntax" \
        "make -f Makefile.kernel -n all > /dev/null"

    # Test 18: Makefile syntax (SMP)
    run_test "SMP Makefile syntax" \
        "make -f Makefile.smp -n all > /dev/null"

    # Test 19: Git repository status
    if git rev-parse --git-dir > /dev/null 2>&1; then
        run_test "Git repository clean" \
            "git diff --quiet || test -z \"\$(git status --porcelain)\""
    fi
}

# ============================================================================
# PERFORMANCE TESTS
# ============================================================================

test_performance() {
    print_header "PERFORMANCE VALIDATION"

    # Test 20: SMP simulator performance
    if [ -f kernel_smp_simulator ]; then
        output=$(./kernel_smp_simulator 2>&1)

        # Extract context switch count
        switches=$(echo "$output" | grep -oP 'CPU\d+: \K\d+(?= context switches)' | head -1)
        if [ -n "$switches" ] && [ "$switches" -gt 1000 ]; then
            ((TESTS_TOTAL++))
            pass_test "High context switch rate ($switches switches in 2s)"
        else
            ((TESTS_TOTAL++))
            fail_test "Low context switch rate ($switches switches in 2s)"
        fi

        # Check for zero spinlock deadlocks
        contentions=$(echo "$output" | grep -oP 'Spinlock Contentions:\s+\K\d+')
        if [ "$contentions" = "0" ]; then
            ((TESTS_TOTAL++))
            pass_test "Zero spinlock contentions"
        else
            ((TESTS_TOTAL++))
            echo -e "${YELLOW}⊙ WARN${NC}: $contentions spinlock contentions detected"
        fi
    fi
}

# ============================================================================
# MAIN EXECUTION
# ============================================================================

main() {
    echo -e "${BLUE}"
    echo "╔══════════════════════════════════════════════════════════════╗"
    echo "║    Ada83 ARM Microkernel - Comprehensive Test Suite         ║"
    echo "╚══════════════════════════════════════════════════════════════╝"
    echo -e "${NC}"

    START_TIME=$(date +%s)

    # Run all test suites
    test_base_kernel
    test_smp_kernel
    test_code_quality
    test_documentation
    test_integration
    test_performance

    END_TIME=$(date +%s)
    DURATION=$((END_TIME - START_TIME))

    # Print final results
    echo -e "\n${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║                    TEST SUMMARY                              ║${NC}"
    echo -e "${BLUE}╠══════════════════════════════════════════════════════════════╣${NC}"
    echo -e "${BLUE}║${NC} Total Tests:    ${TESTS_TOTAL}"
    echo -e "${BLUE}║${NC} ${GREEN}Passed:${NC}         ${TESTS_PASSED}"
    echo -e "${BLUE}║${NC} ${RED}Failed:${NC}         ${TESTS_FAILED}"
    echo -e "${BLUE}║${NC} Duration:       ${DURATION}s"

    # Calculate pass rate
    if [ $TESTS_TOTAL -gt 0 ]; then
        PASS_RATE=$((TESTS_PASSED * 100 / TESTS_TOTAL))
        echo -e "${BLUE}║${NC} Pass Rate:      ${PASS_RATE}%"
    fi

    echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}\n"

    # Exit with appropriate code
    if [ $TESTS_FAILED -eq 0 ]; then
        echo -e "${GREEN}╔══════════════════════════════════════════════════════════════╗${NC}"
        echo -e "${GREEN}║                ALL TESTS PASSED!                             ║${NC}"
        echo -e "${GREEN}║         Microkernel validation successful!                   ║${NC}"
        echo -e "${GREEN}╚══════════════════════════════════════════════════════════════╝${NC}\n"
        exit 0
    else
        echo -e "${RED}╔══════════════════════════════════════════════════════════════╗${NC}"
        echo -e "${RED}║              SOME TESTS FAILED                               ║${NC}"
        echo -e "${RED}║            $TESTS_FAILED / $TESTS_TOTAL tests failed                                ║${NC}"
        echo -e "${RED}╚══════════════════════════════════════════════════════════════╝${NC}\n"
        exit 1
    fi
}

# Run main function
main "$@"
