#!/bin/bash
# ============================================================================
# MICROKERNEL TEST RUNNER
# ============================================================================
# Builds and tests the Ada83 ARM microkernel in QEMU
# ============================================================================

set -e

COLOR_RESET='\033[0m'
COLOR_GREEN='\033[0;32m'
COLOR_BLUE='\033[0;34m'
COLOR_RED='\033[0;31m'
COLOR_YELLOW='\033[1;33m'

print_header() {
    echo -e "${COLOR_BLUE}========================================================================${COLOR_RESET}"
    echo -e "${COLOR_BLUE}$1${COLOR_RESET}"
    echo -e "${COLOR_BLUE}========================================================================${COLOR_RESET}"
}

print_success() {
    echo -e "${COLOR_GREEN}✓ $1${COLOR_RESET}"
}

print_error() {
    echo -e "${COLOR_RED}✗ $1${COLOR_RESET}"
}

print_info() {
    echo -e "${COLOR_YELLOW}→ $1${COLOR_RESET}"
}

# Check for required tools
check_dependencies() {
    print_header "Checking Dependencies"

    local missing=0

    if ! command -v arm-none-eabi-gcc &> /dev/null; then
        print_error "arm-none-eabi-gcc not found"
        missing=1
    else
        print_success "arm-none-eabi-gcc found ($(arm-none-eabi-gcc --version | head -n1))"
    fi

    if ! command -v qemu-system-arm &> /dev/null; then
        print_error "qemu-system-arm not found"
        missing=1
    else
        print_success "qemu-system-arm found ($(qemu-system-arm --version | head -n1))"
    fi

    if [ $missing -eq 1 ]; then
        echo ""
        print_info "Install missing dependencies:"
        echo "  Ubuntu/Debian: sudo apt-get install gcc-arm-none-eabi qemu-system-arm"
        echo "  macOS: brew install arm-none-eabi-gcc qemu"
        exit 1
    fi

    echo ""
}

# Build the microkernel
build_kernel() {
    print_header "Building Ada83 ARM Microkernel"

    print_info "Using Makefile.kernel..."
    make -f Makefile.kernel clean
    make -f Makefile.kernel all

    if [ -f kernel.elf ]; then
        print_success "Microkernel built successfully: kernel.elf"

        # Show size info
        echo ""
        print_info "Kernel size information:"
        ls -lh kernel.elf kernel.bin | awk '{print "  " $5 "\t" $9}'

        echo ""
        print_info "Memory map:"
        head -20 kernel.map
    else
        print_error "Build failed - kernel.elf not found"
        exit 1
    fi

    echo ""
}

# Run in QEMU
run_kernel() {
    print_header "Running Microkernel in QEMU"

    print_info "Starting QEMU virt machine..."
    print_info "Press Ctrl-A then X to exit"
    echo ""

    timeout 10s qemu-system-arm \
        -M virt \
        -cpu cortex-a15 \
        -m 128M \
        -nographic \
        -serial mon:stdio \
        -kernel kernel.elf \
        || true

    echo ""
    print_success "QEMU session ended"
}

# Run in debug mode
debug_kernel() {
    print_header "Starting QEMU Debug Session"

    print_info "QEMU will wait for GDB connection on port 1234"
    print_info "In another terminal, run:"
    echo "  arm-none-eabi-gdb kernel.elf"
    echo "  (gdb) target remote :1234"
    echo "  (gdb) continue"
    echo ""

    qemu-system-arm \
        -M virt \
        -cpu cortex-a15 \
        -m 128M \
        -nographic \
        -serial mon:stdio \
        -kernel kernel.elf \
        -s -S
}

# Disassemble kernel
disassemble_kernel() {
    print_header "Kernel Disassembly"

    if [ -f kernel.elf ]; then
        arm-none-eabi-objdump -D kernel.elf | less
    else
        print_error "kernel.elf not found - build first"
        exit 1
    fi
}

# Show help
show_help() {
    cat << EOF
Ada83 ARM Microkernel Test Runner

Usage: $0 [command]

Commands:
    build       - Build the microkernel
    run         - Build and run in QEMU (default)
    debug       - Build and run in QEMU debug mode
    disasm      - Show kernel disassembly
    clean       - Clean build artifacts
    deps        - Check dependencies
    help        - Show this help message

Examples:
    $0              # Build and run
    $0 build        # Just build
    $0 debug        # Debug with GDB

EOF
}

# Main script
main() {
    local cmd="${1:-run}"

    case "$cmd" in
        build)
            check_dependencies
            build_kernel
            ;;
        run)
            check_dependencies
            build_kernel
            run_kernel
            ;;
        debug)
            check_dependencies
            build_kernel
            debug_kernel
            ;;
        disasm)
            disassemble_kernel
            ;;
        clean)
            make -f Makefile.kernel clean
            print_success "Clean complete"
            ;;
        deps)
            check_dependencies
            ;;
        help|--help|-h)
            show_help
            ;;
        *)
            print_error "Unknown command: $cmd"
            show_help
            exit 1
            ;;
    esac
}

main "$@"
