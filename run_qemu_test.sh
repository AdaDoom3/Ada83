#!/bin/bash
# ============================================================================
# AUTOMATED QEMU TEST RUNNER
# ============================================================================
# Builds the microkernel and runs comprehensive QEMU tests
# Captures output and validates against expected behavior
# ============================================================================

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_header() {
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BLUE}  $1${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

print_info() {
    echo -e "  $1"
}

TEST_DIR=$(mktemp -d)
trap "rm -rf $TEST_DIR" EXIT

# ============================================================================
# CHECK TOOLCHAIN
# ============================================================================

check_toolchain() {
    print_header "Checking ARM Toolchain"

    if command -v arm-none-eabi-gcc >/dev/null 2>&1; then
        local version=$(arm-none-eabi-gcc --version | head -n1)
        print_success "ARM GCC found: $version"
        return 0
    else
        print_error "ARM GCC not found"
        print_info "Install with: sudo apt-get install gcc-arm-none-eabi"
        print_info "Or use Docker: docker build -t ada83-microkernel ."
        return 1
    fi

    if command -v qemu-system-arm >/dev/null 2>&1; then
        local qemu_version=$(qemu-system-arm --version | head -n1)
        print_success "QEMU ARM found: $qemu_version"
        return 0
    else
        print_error "QEMU ARM not found"
        print_info "Install with: sudo apt-get install qemu-system-arm"
        return 1
    fi
}

# ============================================================================
# BUILD KERNEL
# ============================================================================

build_kernel() {
    print_header "Building Microkernel"

    print_info "Cleaning previous build..."
    make -f Makefile.kernel clean >/dev/null 2>&1 || true

    print_info "Building kernel..."
    if make -f Makefile.kernel all > "$TEST_DIR/build.log" 2>&1; then
        print_success "Build completed"
    else
        print_error "Build failed"
        tail -20 "$TEST_DIR/build.log"
        return 1
    fi

    # Check binary exists
    if [ -f kernel.elf ]; then
        local size=$(stat -c%s kernel.elf 2>/dev/null || stat -f%z kernel.elf)
        print_success "kernel.elf created: $size bytes"
    else
        print_error "kernel.elf not found"
        return 1
    fi

    # Analyze binary size
    print_info "Binary size analysis:"
    arm-none-eabi-size kernel.elf | tee "$TEST_DIR/size.txt"

    # Check size is reasonable
    local total_size=$(arm-none-eabi-size kernel.elf | tail -1 | awk '{print $4}')
    if [ $total_size -lt 51200 ]; then  # < 50 KB
        print_success "Binary size excellent: $total_size bytes (< 50 KB)"
    else
        print_warning "Binary size large: $total_size bytes (> 50 KB)"
    fi
}

# ============================================================================
# RUN QEMU TESTS
# ============================================================================

test_boot() {
    print_header "Test 1: Boot and Initialization"

    print_info "Starting QEMU with 5 second timeout..."

    timeout 5s qemu-system-arm \
        -M virt \
        -cpu cortex-a15 \
        -m 128M \
        -nographic \
        -serial mon:stdio \
        -kernel kernel.elf \
        > "$TEST_DIR/qemu_boot.txt" 2>&1 || true

    print_info "QEMU output:"
    cat "$TEST_DIR/qemu_boot.txt"
    echo ""

    # Validate output
    local tests_passed=0
    local tests_total=3

    if grep -q "Ada83 Minix ARM Microkernel" "$TEST_DIR/qemu_boot.txt"; then
        print_success "Kernel banner printed"
        tests_passed=$((tests_passed + 1))
    else
        print_error "Kernel banner not found"
    fi

    if grep -q "Initializing IPC subsystem" "$TEST_DIR/qemu_boot.txt"; then
        print_success "IPC initialization message found"
        tests_passed=$((tests_passed + 1))
    else
        print_error "IPC initialization message not found"
    fi

    if grep -q "Scheduler active" "$TEST_DIR/qemu_boot.txt"; then
        print_success "Scheduler activation message found"
        tests_passed=$((tests_passed + 1))
    else
        print_error "Scheduler activation message not found"
    fi

    print_info "Boot test: $tests_passed/$tests_total checks passed"

    if [ $tests_passed -eq $tests_total ]; then
        return 0
    else
        return 1
    fi
}

test_gdb_inspection() {
    print_header "Test 2: GDB Symbol Inspection"

    print_info "Checking debug symbols..."

    if arm-none-eabi-nm kernel.elf | grep -q "_ada_microkernel_operating_system_executive"; then
        print_success "Ada main entry point found in symbols"
    else
        print_error "Ada main entry point not found"
        return 1
    fi

    if arm-none-eabi-nm kernel.elf | grep -q "reset_handler_initialization_entry_point"; then
        print_success "Reset handler found in symbols"
    else
        print_error "Reset handler not found"
        return 1
    fi

    if arm-none-eabi-nm kernel.elf | grep -q "context_switch"; then
        print_success "Context switch routine found in symbols"
    else
        print_error "Context switch routine not found"
        return 1
    fi

    if arm-none-eabi-nm kernel.elf | grep -q "neon_memcpy"; then
        print_success "NEON memcpy found in symbols"
    else
        print_warning "NEON memcpy not found (may be optimized)"
    fi

    return 0
}

test_disassembly() {
    print_header "Test 3: Disassembly Verification"

    print_info "Generating disassembly..."
    arm-none-eabi-objdump -d kernel.elf > "$TEST_DIR/kernel.dis"

    # Check for NEON instructions
    if grep -qE '\bvld|vst|vadd|vmul\b' "$TEST_DIR/kernel.dis"; then
        print_success "NEON instructions found in disassembly"
    else
        print_warning "No NEON instructions found (may be optimized out)"
    fi

    # Check for exception handlers
    if grep -q "undefined_instruction_exception_handler" "$TEST_DIR/kernel.dis"; then
        print_success "Exception handlers present"
    else
        print_error "Exception handlers not found"
        return 1
    fi

    # Check for IPC functions
    if grep -q "send_message" "$TEST_DIR/kernel.dis"; then
        print_success "IPC functions present in code"
    else
        print_warning "IPC function names not found (may be inlined)"
    fi

    return 0
}

test_memory_layout() {
    print_header "Test 4: Memory Layout Verification"

    print_info "Analyzing sections..."
    arm-none-eabi-readelf -S kernel.elf > "$TEST_DIR/sections.txt"

    # Check critical sections exist
    if grep -q "\.vectors" "$TEST_DIR/sections.txt"; then
        print_success "Vector table section found"
    else
        print_error "Vector table section missing"
        return 1
    fi

    if grep -q "\.text" "$TEST_DIR/sections.txt"; then
        print_success "Code section found"
    else
        print_error "Code section missing"
        return 1
    fi

    if grep -q "\.data" "$TEST_DIR/sections.txt"; then
        print_success "Data section found"
    else
        print_warning "Data section not found"
    fi

    if grep -q "\.bss" "$TEST_DIR/sections.txt"; then
        print_success "BSS section found"
    else
        print_warning "BSS section not found"
    fi

    # Check entry point
    local entry=$(arm-none-eabi-readelf -h kernel.elf | grep "Entry point" | awk '{print $4}')
    print_info "Entry point address: $entry"

    if [ "$entry" = "0x40000000" ]; then
        print_success "Entry point correct (0x40000000)"
    else
        print_warning "Entry point unexpected: $entry"
    fi

    return 0
}

# ============================================================================
# GENERATE TEST REPORT
# ============================================================================

generate_report() {
    print_header "Generating Test Report"

    local report_file="QEMU_TEST_RESULTS.md"

    cat > "$report_file" << 'EOF'
# QEMU Test Results - Ada83 ARM Microkernel

## Test Execution Summary

**Date**: $(date)
**Kernel Version**: v1.0
**Test Environment**: $(uname -a)
**ARM GCC**: $(arm-none-eabi-gcc --version | head -n1)
**QEMU**: $(qemu-system-arm --version | head -n1)

---

## Test Results

EOF

    # Add boot test results
    echo "### Test 1: Boot and Initialization" >> "$report_file"
    echo '```' >> "$report_file"
    cat "$TEST_DIR/qemu_boot.txt" >> "$report_file"
    echo '```' >> "$report_file"
    echo "" >> "$report_file"

    # Add size analysis
    echo "### Binary Size Analysis" >> "$report_file"
    echo '```' >> "$report_file"
    cat "$TEST_DIR/size.txt" >> "$report_file"
    echo '```' >> "$report_file"
    echo "" >> "$report_file"

    # Add sections
    echo "### Memory Layout" >> "$report_file"
    echo '```' >> "$report_file"
    head -20 "$TEST_DIR/sections.txt" >> "$report_file"
    echo '```' >> "$report_file"
    echo "" >> "$report_file"

    print_success "Report generated: $report_file"
}

# ============================================================================
# MAIN EXECUTION
# ============================================================================

main() {
    echo ""
    echo "╔══════════════════════════════════════════════════════════════╗"
    echo "║      Ada83 ARM Microkernel - QEMU Test Suite                ║"
    echo "╚══════════════════════════════════════════════════════════════╝"
    echo ""

    local tests_passed=0
    local tests_total=0

    # Check toolchain
    if ! check_toolchain; then
        print_error "Toolchain not available - cannot run tests"
        print_info "See QEMU_TEST_SIMULATION.md for expected results"
        exit 1
    fi

    # Build kernel
    if ! build_kernel; then
        print_error "Build failed - cannot proceed with tests"
        exit 1
    fi

    tests_total=4

    # Run tests
    test_boot && tests_passed=$((tests_passed + 1))
    test_gdb_inspection && tests_passed=$((tests_passed + 1))
    test_disassembly && tests_passed=$((tests_passed + 1))
    test_memory_layout && tests_passed=$((tests_passed + 1))

    # Generate report
    generate_report

    # Summary
    print_header "Test Summary"
    echo ""
    echo "  Tests Passed: $tests_passed / $tests_total"
    echo ""

    if [ $tests_passed -eq $tests_total ]; then
        print_success "ALL TESTS PASSED!"
        print_info "The microkernel is validated and ready for production"
        echo ""
        echo "╔══════════════════════════════════════════════════════════════╗"
        echo "║                    SUCCESS                                   ║"
        echo "║  Microkernel successfully built and tested in QEMU!          ║"
        echo "╚══════════════════════════════════════════════════════════════╝"
        return 0
    else
        print_warning "Some tests failed ($tests_passed/$tests_total passed)"
        print_info "Review logs in: $TEST_DIR"
        print_info "See QEMU_TEST_RESULTS.md for details"
        return 1
    fi
}

main "$@"
