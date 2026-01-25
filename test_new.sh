#!/bin/bash
# Test script for ada83new.c - compares with ada83.c
set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Counters
declare -A OLD=([pass]=0 [fail]=0 [skip]=0 [total]=0)
declare -A NEW=([pass]=0 [fail]=0 [skip]=0 [total]=0)

mkdir -p test_results_old test_results_new acats_logs_old acats_logs_new

# Test a single file with both compilers
test_file() {
    local f=$1
    local n=$(basename "$f" .ada)
    local class=${n:0:1}

    # Skip non-test files
    [[ $n =~ [0-9]$ && ! $n =~ m$ ]] && return
    [[ $class == [fF] ]] && return  # Foundation support code

    ((++OLD[total]))
    ((++NEW[total]))

    local old_compile=0 new_compile=0
    local old_link=0 new_link=0
    local old_run=0 new_run=0

    # Test with old compiler (ada83)
    if timeout 0.5 ./ada83 -Iacats -Irts "$f" > test_results_old/$n.ll 2>acats_logs_old/$n.err; then
        old_compile=1
        if timeout 0.5 llvm-link -o test_results_old/$n.bc test_results_old/$n.ll rts/report_old.ll 2>/dev/null; then
            old_link=1
            if timeout 1 lli test_results_old/$n.bc > acats_logs_old/$n.out 2>&1; then
                grep -q PASSED acats_logs_old/$n.out 2>/dev/null && old_run=1
            fi
        fi
    fi

    # Test with new compiler (ada83new)
    if timeout 0.5 ./ada83new -Iacats -Irts "$f" > test_results_new/$n.ll 2>acats_logs_new/$n.err; then
        new_compile=1
        if timeout 0.5 llvm-link -o test_results_new/$n.bc test_results_new/$n.ll rts/report_new.ll 2>/dev/null; then
            new_link=1
            if timeout 1 lli test_results_new/$n.bc > acats_logs_new/$n.out 2>&1; then
                grep -q PASSED acats_logs_new/$n.out 2>/dev/null && new_run=1
            fi
        fi
    fi

    # Determine pass/fail for B-tests (should reject)
    if [[ $class == [bB] ]]; then
        if ((old_compile == 0)); then OLD[pass]=$((OLD[pass]+1)); else OLD[fail]=$((OLD[fail]+1)); fi
        if ((new_compile == 0)); then NEW[pass]=$((NEW[pass]+1)); else NEW[fail]=$((NEW[fail]+1)); fi
        local old_status=$([[ $old_compile == 0 ]] && echo "REJECT" || echo "WRONG")
        local new_status=$([[ $new_compile == 0 ]] && echo "REJECT" || echo "WRONG")
    else
        # For other tests, need to compile, link, and pass
        if ((old_run)); then OLD[pass]=$((OLD[pass]+1)); else OLD[fail]=$((OLD[fail]+1)); fi
        if ((new_run)); then NEW[pass]=$((NEW[pass]+1)); else NEW[fail]=$((NEW[fail]+1)); fi
        local old_status=$([[ $old_run == 1 ]] && echo "PASS" || echo "FAIL")
        local new_status=$([[ $new_run == 1 ]] && echo "PASS" || echo "FAIL")
    fi

    # Print comparison
    local comp_mark=""
    if [[ $old_status == $new_status ]]; then
        comp_mark="="
    elif [[ $new_status == "PASS" || $new_status == "REJECT" ]]; then
        comp_mark="${GREEN}+${NC}"
    else
        comp_mark="${RED}-${NC}"
    fi

    printf "  %-16s  OLD: %-6s  NEW: %-6s  %b\n" "$n" "$old_status" "$new_status" "$comp_mark"
}

# Build report.ll files if needed (one for each compiler since they use different symbol naming)
build_report() {
    if [[ ! -f rts/report_old.ll || rts/report.adb -nt rts/report_old.ll ]]; then
        echo "Building rts/report_old.ll (old compiler)..."
        ./ada83 -Iacats -Irts rts/report.adb > rts/report_old.ll 2>/dev/null || true
    fi
    if [[ ! -f rts/report_new.ll || rts/report.adb -nt rts/report_new.ll ]]; then
        echo "Building rts/report_new.ll (new compiler)..."
        ./ada83new -Iacats -Irts rts/report.adb > rts/report_new.ll 2>/dev/null || true
    fi
}

# Run tests for a class
run_class() {
    local class=$1
    echo ""
    echo "========================================"
    echo " Class ${class^^} Tests"
    echo "========================================"
    echo ""

    for f in acats/${class,,}*.ada; do
        [[ -f $f ]] && test_file "$f"
    done
}

# Print summary
print_summary() {
    echo ""
    echo "========================================"
    echo " SUMMARY"
    echo "========================================"
    echo ""
    printf "  %-20s %8s %8s\n" "" "OLD" "NEW"
    printf "  %-20s %8s %8s\n" "--------------------" "--------" "--------"
    printf "  %-20s %8d %8d\n" "Passed" ${OLD[pass]} ${NEW[pass]}
    printf "  %-20s %8d %8d\n" "Failed" ${OLD[fail]} ${NEW[fail]}
    printf "  %-20s %8d %8d\n" "Total" ${OLD[total]} ${NEW[total]}
    printf "  %-20s %7d%% %7d%%\n" "Pass Rate" $((100*OLD[pass]/OLD[total])) $((100*NEW[pass]/NEW[total]))
    echo ""

    local diff=$((NEW[pass] - OLD[pass]))
    if ((diff > 0)); then
        echo -e "  ${GREEN}NEW is ahead by $diff tests${NC}"
    elif ((diff < 0)); then
        echo -e "  ${RED}NEW is behind by $((-diff)) tests${NC}"
    else
        echo -e "  ${BLUE}Both compilers are equal${NC}"
    fi
    echo ""
}

# Quick test - just a few tests
quick_test() {
    echo ""
    echo "========================================"
    echo " Quick Comparison Test"
    echo "========================================"

    build_report

    # Test a sample from each class
    for f in acats/a21001a.ada acats/b22001a.ada acats/c23001a.ada acats/c32001a.ada acats/c34001a.ada; do
        [[ -f $f ]] && test_file "$f"
    done

    print_summary
}

# Full test
full_test() {
    build_report

    for class in a b c d e l; do
        run_class $class
    done

    print_summary
}

# Single class test
class_test() {
    build_report
    run_class "$1"
    print_summary
}

# Usage
usage() {
    cat << EOF
Usage: $0 <mode> [class]

Modes:
  quick       Run a quick sample comparison
  full        Run full test suite comparison
  class X     Run tests for class X (a/b/c/d/e/l)
  help        Show this help

Compares ada83 (original) vs ada83new (rewrite)
EOF
}

case ${1:-help} in
    quick) quick_test ;;
    full) full_test ;;
    class) class_test "${2:-c}" ;;
    help|*) usage ;;
esac
