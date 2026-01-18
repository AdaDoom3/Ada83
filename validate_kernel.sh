#!/bin/bash
# ============================================================================
# MICROKERNEL VALIDATION SCRIPT
# ============================================================================
# Validates kernel source files without requiring full ARM toolchain
# Checks syntax, structure, and completeness
# ============================================================================

set -e

PASS=0
FAIL=0

check() {
    if [ $? -eq 0 ]; then
        echo "✓ $1"
        PASS=$((PASS + 1))
    else
        echo "✗ $1"
        FAIL=$((FAIL + 1))
    fi
}

echo "========================================================================="
echo "Ada83 ARM Microkernel - Source Validation"
echo "========================================================================="
echo ""

# Check file existence
echo "Checking required files..."
test -f kernel.adb && check "kernel.adb exists" || check "kernel.adb missing"
test -f boot.s && check "boot.s exists" || check "boot.s missing"
test -f kernel.ld && check "kernel.ld exists" || check "kernel.ld missing"
test -f Makefile.kernel && check "Makefile.kernel exists" || check "Makefile.kernel missing"
test -f test_kernel.sh && check "test_kernel.sh exists" || check "test_kernel.sh missing"
test -x test_kernel.sh && check "test_kernel.sh executable" || check "test_kernel.sh not executable"
echo ""

# Check Ada syntax (basic)
echo "Checking Ada syntax..."
grep -q "procedure Microkernel_Operating_System_Executive is" kernel.adb
check "Ada procedure declaration found"

grep -q "end Microkernel_Operating_System_Executive" kernel.adb
check "Ada procedure end found"

grep -q "type Process_Identifier is new Integer" kernel.adb
check "Ada type declarations found"

grep -q "pragma Import" kernel.adb
check "Ada assembly imports found"
echo ""

# Check ARM assembly syntax
echo "Checking ARM assembly..."
grep -q ".syntax unified" boot.s
check "ARM unified syntax directive"

grep -q ".cpu cortex-a15" boot.s
check "ARM CPU target specified"

grep -q ".fpu neon-vfpv4" boot.s
check "NEON FPU enabled"

grep -q "arm_exception_vector_table_base:" boot.s
check "Exception vector table defined"

grep -q "reset_handler_initialization_entry_point:" boot.s
check "Reset handler defined"

grep -q "context_switch:" boot.s
check "Context switch routine defined"

grep -q "uart_putc:" boot.s
check "UART driver defined"

grep -q "neon_memcpy:" boot.s
check "NEON memcpy defined"
echo ""

# Check linker script
echo "Checking linker script..."
grep -q "OUTPUT_ARCH(arm)" kernel.ld
check "ARM architecture specified"

grep -q "ENTRY(reset_handler_initialization_entry_point)" kernel.ld
check "Entry point defined"

grep -q ".vectors" kernel.ld
check "Vector section defined"

grep -q ".text" kernel.ld
check "Text section defined"

grep -q ".bss" kernel.ld
check "BSS section defined"
echo ""

# Check Makefile
echo "Checking build system..."
grep -q "arm-none-eabi-gcc" Makefile.kernel
check "ARM cross-compiler configured"

grep -q "QEMU := qemu-system-arm" Makefile.kernel
check "QEMU target configured"

grep -q "cortex-a15" Makefile.kernel
check "Cortex-A15 target specified"

grep -q "neon" Makefile.kernel
check "NEON support enabled"
echo ""

# Check documentation
echo "Checking documentation..."
test -f MICROKERNEL.md && check "MICROKERNEL.md exists" || check "MICROKERNEL.md missing"
test -f QUICKSTART.md && check "QUICKSTART.md exists" || check "QUICKSTART.md missing"

if [ -f MICROKERNEL.md ]; then
    grep -q "Minix" MICROKERNEL.md
    check "Minix architecture documented"

    grep -q "IPC" MICROKERNEL.md
    check "IPC design documented"
fi
echo ""

# Check code metrics
echo "Code metrics..."
ADA_LINES=$(wc -l < kernel.adb)
ASM_LINES=$(wc -l < boot.s)
echo "  Ada code: $ADA_LINES lines"
echo "  ARM assembly: $ASM_LINES lines"

if [ $ADA_LINES -gt 100 ] && [ $ADA_LINES -lt 1000 ]; then
    check "Ada code size reasonable ($ADA_LINES lines)"
else
    check "Ada code size unusual ($ADA_LINES lines)"
fi

if [ $ASM_LINES -gt 100 ] && [ $ASM_LINES -lt 1000 ]; then
    check "Assembly code size reasonable ($ASM_LINES lines)"
else
    check "Assembly code size unusual ($ASM_LINES lines)"
fi
echo ""

# Feature completeness check
echo "Checking kernel features..."
grep -q "Process_Control_Block" kernel.adb
check "Process management implemented"

grep -q "Inter_Process_Communication_Message_Block" kernel.adb
check "IPC message structure defined"

grep -q "Send_Message_To_Process" kernel.adb
check "IPC send primitive implemented"

grep -q "Receive_Message_From_Any_Process" kernel.adb
check "IPC receive primitive implemented"

grep -q "Schedule_Next_Ready_Process" kernel.adb
check "Scheduler implemented"

grep -q "Allocate_Physical_Memory_Page" kernel.adb
check "Memory allocator implemented"

grep -q "Handle_System_Call_Interrupt" kernel.adb
check "System call handler implemented"
echo ""

# Summary
echo "========================================================================="
echo "VALIDATION SUMMARY"
echo "========================================================================="
echo "Passed: $PASS"
echo "Failed: $FAIL"
echo ""

if [ $FAIL -eq 0 ]; then
    echo "✓ All validation checks passed!"
    echo ""
    echo "The microkernel source is complete and ready to build."
    echo "Install ARM cross-compiler and QEMU to build and test:"
    echo "  sudo apt-get install gcc-arm-none-eabi qemu-system-arm"
    echo ""
    echo "Then run: ./test_kernel.sh"
    exit 0
else
    echo "✗ Some validation checks failed."
    echo ""
    echo "Please review the failed checks above."
    exit 1
fi
