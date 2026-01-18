/*
 * ============================================================================
 * ARM ASSEMBLY FOR SMP-ENHANCED MICROKERNEL
 * ============================================================================
 * Additional SMP-specific assembly routines:
 * - Spinlock primitives (LDREX/STREX)
 * - CPU ID detection (MPIDR)
 * - Secondary CPU boot sequence
 * - Inter-processor interrupts (IPI)
 * - Memory barriers (DMB/DSB/ISB)
 *
 * Target: ARMv7-A MP (Cortex-A15 quad-core)
 * ============================================================================
 */

.syntax unified
.cpu cortex-a15
.fpu neon-vfpv4
.arm

/* ========================================================================== */
/* SMP CONSTANTS                                                              */
/* ========================================================================== */

.equ MAX_CPUS, 4
.equ GIC_DIST_BASE, 0x08000000     /* QEMU virt GIC distributor */
.equ GIC_CPU_BASE, 0x08010000      /* QEMU virt GIC CPU interface */

/* ========================================================================== */
/* SPINLOCK PRIMITIVES (Using LDREX/STREX)                                   */
/* ========================================================================== */

.global spinlock_acquire
spinlock_acquire:
    /* r0 = address of spinlock (32-bit integer) */
    dmb sy                          /* Memory barrier before acquiring */

spinlock_acquire_loop:
    ldrex r1, [r0]                  /* Load-exclusive from lock */
    cmp r1, #0                      /* Check if lock is free (0 = free) */
    wfene                           /* If locked, wait for event */
    bne spinlock_acquire_loop       /* Retry if locked */

    mov r1, #1                      /* Prepare to set lock to 1 (locked) */
    strex r2, r1, [r0]              /* Store-exclusive to lock */
    cmp r2, #0                      /* Check if store succeeded */
    bne spinlock_acquire_loop       /* Retry if store failed */

    dmb sy                          /* Memory barrier after acquiring */
    bx lr

.global spinlock_release
spinlock_release:
    /* r0 = address of spinlock */
    dmb sy                          /* Memory barrier before releasing */

    mov r1, #0                      /* Prepare to clear lock */
    str r1, [r0]                    /* Store 0 (unlock) */

    dsb sy                          /* Data synchronization barrier */
    sev                             /* Send event to wake up waiting CPUs */

    bx lr

/* ========================================================================== */
/* CPU ID DETECTION (Read MPIDR register)                                    */
/* ========================================================================== */

.global get_cpu_id
get_cpu_id:
    /* Returns CPU ID in r0 (0-3 for quad-core) */
    mrc p15, 0, r0, c0, c0, 5       /* Read MPIDR (Multiprocessor Affinity Register) */
    and r0, r0, #0x3                /* Extract CPU ID (bits [1:0]) */
    bx lr

/* ========================================================================== */
/* MEMORY BARRIERS                                                            */
/* ========================================================================== */

.global dmb_full
dmb_full:
    dmb sy                          /* Full system data memory barrier */
    bx lr

.global dsb_full
dsb_full:
    dsb sy                          /* Full system data synchronization barrier */
    bx lr

.global isb_full
isb_full:
    isb sy                          /* Instruction synchronization barrier */
    bx lr

/* ========================================================================== */
/* SECONDARY CPU BOOT SEQUENCE                                                */
/* ========================================================================== */

.global secondary_cpu_boot
secondary_cpu_boot:
    /* Disable interrupts */
    cpsid if

    /* Read CPU ID */
    bl get_cpu_id
    mov r4, r0                      /* Save CPU ID in r4 */

    /* Set up stack for this CPU (each CPU gets 16KB stack) */
    ldr r1, =secondary_cpu_stack_base
    add r1, r1, r4, lsl #14         /* offset = CPU_ID * 16384 */
    add sp, r1, #0x4000             /* SP = base + 16KB */

    /* Initialize per-CPU GIC interface */
    bl init_gic_cpu_interface

    /* Clear BSS if needed (already done by primary CPU) */

    /* Enable NEON/VFP for this CPU */
    mrc p15, 0, r0, c1, c0, 2       /* Read CPACR */
    orr r0, r0, #(0xF << 20)        /* Enable CP10 & CP11 */
    mcr p15, 0, r0, c1, c0, 2       /* Write CPACR */
    isb
    mov r0, #0x40000000
    vmsr fpexc, r0

    /* Set vector table */
    ldr r0, =arm_exception_vector_table_base
    mcr p15, 0, r0, c12, c0, 0      /* Set VBAR */

    /* Jump to Ada secondary CPU entry point */
    mov r0, r4                      /* Pass CPU ID as parameter */
    bl _ada_secondary_cpu_entry

    /* Should never return, but if it does, halt */
secondary_cpu_halt:
    wfi
    b secondary_cpu_halt

/* ========================================================================== */
/* INTER-PROCESSOR INTERRUPTS (IPI)                                          */
/* ========================================================================== */

.global send_ipi
send_ipi:
    /* r0 = target CPU ID (0-3) */
    /* Send SGI (Software Generated Interrupt) to target CPU */

    push {r4, lr}

    /* SGI register in GIC distributor: GICD_SGIR */
    ldr r1, =GIC_DIST_BASE
    add r1, r1, #0xF00              /* GICD_SGIR offset */

    /* Prepare SGI value:
     * [25:24] = target filter (00 = target list)
     * [23:16] = CPU target list (1 << target_cpu)
     * [3:0]   = SGI interrupt ID (use ID 0)
     */
    mov r2, #1
    lsl r2, r2, r0                  /* 1 << target_cpu */
    lsl r2, r2, #16                 /* Shift to CPU target list field */
    orr r2, r2, #0                  /* SGI ID = 0 */

    str r2, [r1]                    /* Write to GICD_SGIR */

    dsb sy                          /* Ensure write completes */

    pop {r4, pc}

/* ========================================================================== */
/* GIC INITIALIZATION (Per-CPU)                                              */
/* ========================================================================== */

init_gic_cpu_interface:
    push {r4, lr}

    ldr r0, =GIC_CPU_BASE

    /* Set priority mask to allow all interrupts */
    mov r1, #0xFF
    str r1, [r0, #0x04]             /* GICC_PMR */

    /* Enable CPU interface */
    mov r1, #1
    str r1, [r0, #0x00]             /* GICC_CTLR */

    pop {r4, pc}

/* ========================================================================== */
/* ENHANCED CONTEXT SWITCH (SMP-aware)                                       */
/* ========================================================================== */

.global context_switch_smp
context_switch_smp:
    /* r0 = target process ID */
    /* Save current process context */
    push {r0-r12, lr}

    /* Get CPU ID */
    bl get_cpu_id
    mov r4, r0                      /* r4 = CPU ID */

    /* Load new process context from process table */
    /* Address = process_table_base + (process_id * 128) */
    ldr r1, =current_process_context_save_area
    ldr r0, [sp, #0]                /* Restore original r0 (process ID) */
    add r1, r1, r0, lsl #7          /* 128 bytes per context */
    ldmia r1, {r0-r12, lr}

    /* Memory barrier to ensure context is loaded */
    dmb sy

    /* Return to new process */
    pop {pc}

/* ========================================================================== */
/* ATOMIC OPERATIONS                                                          */
/* ========================================================================== */

.global atomic_add
atomic_add:
    /* r0 = address, r1 = value to add */
    /* Returns old value in r0 */
atomic_add_loop:
    ldrex r2, [r0]
    add r3, r2, r1
    strex r12, r3, [r0]
    cmp r12, #0
    bne atomic_add_loop
    mov r0, r2                      /* Return old value */
    bx lr

.global atomic_inc
atomic_inc:
    /* r0 = address */
    /* Returns new value in r0 */
    mov r1, #1
    b atomic_add

.global atomic_dec
atomic_dec:
    /* r0 = address */
    /* Returns new value in r0 */
    mvn r1, #0                      /* r1 = -1 */
    b atomic_add

.global atomic_cmpxchg
atomic_cmpxchg:
    /* r0 = address, r1 = old_value, r2 = new_value */
    /* Returns 1 if successful, 0 if failed */
atomic_cmpxchg_loop:
    ldrex r3, [r0]
    cmp r3, r1
    bne atomic_cmpxchg_fail         /* Value changed, fail */

    strex r12, r2, [r0]
    cmp r12, #0
    bne atomic_cmpxchg_loop         /* Retry if store failed */

    mov r0, #1                      /* Success */
    bx lr

atomic_cmpxchg_fail:
    clrex                           /* Clear exclusive monitor */
    mov r0, #0                      /* Failure */
    bx lr

/* ========================================================================== */
/* PER-CPU DATA ACCESS                                                        */
/* ========================================================================== */

.global get_percpu_data_address
get_percpu_data_address:
    /* Returns address of per-CPU data for current CPU in r0 */
    bl get_cpu_id
    ldr r1, =per_cpu_data_base
    add r0, r1, r0, lsl #6          /* 64 bytes per CPU structure */
    bx lr

/* ========================================================================== */
/* CACHE OPERATIONS (for SMP coherency)                                      */
/* ========================================================================== */

.global flush_cache_line
flush_cache_line:
    /* r0 = address */
    mcr p15, 0, r0, c7, c10, 1      /* Clean data cache line by MVA */
    dsb sy
    bx lr

.global invalidate_cache_line
invalidate_cache_line:
    /* r0 = address */
    mcr p15, 0, r0, c7, c6, 1       /* Invalidate data cache line by MVA */
    dsb sy
    bx lr

.global flush_tlb
flush_tlb:
    mov r0, #0
    mcr p15, 0, r0, c8, c7, 0       /* Invalidate entire TLB */
    dsb sy
    isb sy
    bx lr

/* ========================================================================== */
/* PERFORMANCE COUNTERS (for benchmarking)                                   */
/* ========================================================================== */

.global enable_cycle_counter
enable_cycle_counter:
    /* Enable Performance Monitor User Enable Register */
    mrc p15, 0, r0, c9, c14, 0      /* Read PMUSERENR */
    orr r0, r0, #1                  /* Enable user-mode access */
    mcr p15, 0, r0, c9, c14, 0      /* Write PMUSERENR */

    /* Enable cycle counter */
    mrc p15, 0, r0, c9, c12, 0      /* Read PMCR */
    orr r0, r0, #1                  /* Enable all counters */
    orr r0, r0, #4                  /* Reset cycle counter */
    mcr p15, 0, r0, c9, c12, 0      /* Write PMCR */

    /* Enable cycle counter specifically */
    mov r0, #0x80000000
    mcr p15, 0, r0, c9, c12, 1      /* PMCNTENSET */

    bx lr

.global read_cycle_counter
read_cycle_counter:
    /* Returns cycle count in r0 */
    mrc p15, 0, r0, c9, c13, 0      /* Read PMCCNTR */
    bx lr

.global reset_cycle_counter
reset_cycle_counter:
    mrc p15, 0, r0, c9, c12, 0
    orr r0, r0, #4                  /* Reset cycle counter bit */
    mcr p15, 0, r0, c9, c12, 0
    bx lr

/* ========================================================================== */
/* DATA SECTION - SMP-specific storage                                       */
/* ========================================================================== */

.section .bss
.align 12                           /* 4KB alignment for stacks */

secondary_cpu_stack_base:
    .space 0x10000                  /* 64KB total (16KB per CPU × 4 CPUs) */

per_cpu_data_base:
    .space 256                      /* 64 bytes per CPU × 4 CPUs */

/* Spinlock pool for kernel subsystems */
.global kernel_spinlocks
kernel_spinlocks:
    .space 64                       /* 16 spinlocks × 4 bytes */

.section .data
.global cpu_online_mask
cpu_online_mask:
    .word 0x01                      /* Bitmap of online CPUs (CPU 0 initially) */

.end
