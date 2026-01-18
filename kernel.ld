/*
 * ============================================================================
 * LINKER SCRIPT FOR ARM BARE-METAL MICROKERNEL
 * ============================================================================
 * Target: ARMv7-A (QEMU virt machine)
 * Memory Layout:
 *   - Vectors at 0x40000000 (QEMU RAM base)
 *   - Code follows vectors
 *   - Data and BSS after code
 *   - Stack at top of RAM
 * ============================================================================
 */

OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(reset_handler_initialization_entry_point)

MEMORY
{
    /* QEMU virt machine RAM layout */
    RAM (rwx) : ORIGIN = 0x40000000, LENGTH = 128M
}

SECTIONS
{
    /* Exception vectors must be first */
    .vectors : ALIGN(4)
    {
        KEEP(*(.vectors))
    } > RAM

    /* Code section */
    .text : ALIGN(4)
    {
        *(.text.reset_handler_initialization_entry_point)
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

    /* Initialized data */
    .data : ALIGN(4)
    {
        __data_start__ = .;
        *(.data*)
        __data_end__ = .;
    } > RAM

    /* Zero-initialized data */
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

    /* Stack grows downward from end of RAM */
    .stack (NOLOAD) : ALIGN(8)
    {
        __stack_bottom__ = .;
        . = . + 0x10000;    /* 64KB stack */
        __stack_top__ = .;
    } > RAM

    /* Heap for dynamic allocation */
    .heap (NOLOAD) : ALIGN(8)
    {
        __heap_start__ = .;
        . = ORIGIN(RAM) + LENGTH(RAM) - 0x100000;  /* Leave 1MB for heap */
        __heap_end__ = .;
    } > RAM

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
