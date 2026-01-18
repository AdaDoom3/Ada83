/*
 * ============================================================================
 * LINKER SCRIPT FOR ARM SMP MICROKERNEL WITH MULTICORE SUPPORT
 * ============================================================================
 * Target: ARMv7-A with SMP (up to 4 CPUs)
 * Memory Layout:
 *   - Vectors at 0x40000000 (QEMU RAM base)
 *   - Code follows vectors
 *   - Data and BSS after code
 *   - Per-CPU data regions (cache-line aligned)
 *   - Per-CPU stacks (64KB each)
 *   - Shared heap for dynamic allocation
 * ============================================================================
 */

OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(reset_handler_initialization_entry_point)

MEMORY
{
    /* QEMU virt machine RAM layout - 128MB total */
    RAM (rwx) : ORIGIN = 0x40000000, LENGTH = 128M
}

SECTIONS
{
    /* Exception vectors must be first - shared by all CPUs */
    .vectors : ALIGN(4)
    {
        KEEP(*(.vectors))
    } > RAM

    /* Code section - shared by all CPUs */
    .text : ALIGN(4)
    {
        *(.text.reset_handler_initialization_entry_point)
        *(.text.secondary_cpu_entry_point)
        *(.text*)
        *(.rodata*)
        *(.glue_7)
        *(.glue_7t)
        *(.vfp11_veneer)
        *(.v4_bx)
    } > RAM

    /* ARM exception index (for C++ exceptions, unused but needed) */
    .ARM.exidx : ALIGN(4)
    {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } > RAM

    /* Initialized data - shared by all CPUs */
    .data : ALIGN(4)
    {
        __data_start__ = .;
        *(.data*)
        __data_end__ = .;
    } > RAM

    /* Zero-initialized data - shared by all CPUs */
    .bss : ALIGN(4)
    {
        __bss_start__ = .;
        *(.bss*)
        *(COMMON)
        __bss_end__ = .;
    } > RAM

    /* Ada runtime data sections */
    .ada_elaboration : ALIGN(4)
    {
        *(.ada.*)
    } > RAM

    /* ========================================================================
     * PER-CPU DATA SECTIONS (cache-line aligned for performance)
     * ======================================================================== */

    /* Per-CPU data section - 64-byte cache line alignment */
    .percpu : ALIGN(64)
    {
        __percpu_data_start__ = .;

        /* CPU 0 data region (4KB per CPU) */
        __percpu_cpu0_start__ = .;
        . = . + 0x1000;
        __percpu_cpu0_end__ = .;

        /* CPU 1 data region */
        . = ALIGN(64);
        __percpu_cpu1_start__ = .;
        . = . + 0x1000;
        __percpu_cpu1_end__ = .;

        /* CPU 2 data region */
        . = ALIGN(64);
        __percpu_cpu2_start__ = .;
        . = . + 0x1000;
        __percpu_cpu2_end__ = .;

        /* CPU 3 data region */
        . = ALIGN(64);
        __percpu_cpu3_start__ = .;
        . = . + 0x1000;
        __percpu_cpu3_end__ = .;

        __percpu_data_end__ = .;
    } > RAM

    /* ========================================================================
     * PER-CPU STACKS (separate stack for each CPU)
     * ======================================================================== */

    .stacks (NOLOAD) : ALIGN(8)
    {
        __stacks_start__ = .;

        /* CPU 0 stack (64KB) */
        __stack_cpu0_bottom__ = .;
        . = . + 0x10000;
        __stack_cpu0_top__ = .;

        /* CPU 1 stack (64KB) */
        __stack_cpu1_bottom__ = .;
        . = . + 0x10000;
        __stack_cpu1_top__ = .;

        /* CPU 2 stack (64KB) */
        __stack_cpu2_bottom__ = .;
        . = . + 0x10000;
        __stack_cpu2_top__ = .;

        /* CPU 3 stack (64KB) */
        __stack_cpu3_bottom__ = .;
        . = . + 0x10000;
        __stack_cpu3_top__ = .;

        __stacks_end__ = .;
    } > RAM

    /* Shared heap for dynamic allocation */
    .heap (NOLOAD) : ALIGN(8)
    {
        __heap_start__ = .;
        . = ORIGIN(RAM) + LENGTH(RAM) - 0x200000;  /* Leave 2MB for heap */
        __heap_end__ = .;
    } > RAM

    /* ========================================================================
     * SMP-SPECIFIC METADATA
     * ======================================================================== */

    /* CPU count and configuration */
    __smp_cpu_count__ = 4;
    __smp_percpu_size__ = 0x1000;  /* 4KB per CPU */
    __smp_stack_size__ = 0x10000;   /* 64KB per stack */

    /* CPU stack top addresses for boot initialization */
    __cpu0_stack__ = __stack_cpu0_top__;
    __cpu1_stack__ = __stack_cpu1_top__;
    __cpu2_stack__ = __stack_cpu2_top__;
    __cpu3_stack__ = __stack_cpu3_top__;

    /* Debug sections (not loaded) */
    .comment 0 : { *(.comment) }
    .debug_info 0 : { *(.debug_info) }
    .debug_abbrev 0 : { *(.debug_abbrev) }
    .debug_line 0 : { *(.debug_line) }
    .debug_frame 0 : { *(.debug_frame) }
    .debug_str 0 : { *(.debug_str) }
    .debug_loc 0 : { *(.debug_loc) }
    .debug_ranges 0 : { *(.debug_ranges) }
    .debug_aranges 0 : { *(.debug_aranges) }

    /* Discard unwanted sections */
    /DISCARD/ :
    {
        *(.note.GNU-stack)
        *(.gnu_debuglink)
        *(.gnu.lto_*)
    }
}
