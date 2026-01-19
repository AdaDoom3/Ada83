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
/* NEON-OPTIMIZED FAST PATHS FOR CRITICAL OPERATIONS                         */
/* ========================================================================== */

.global neon_copy_ipc_message
neon_copy_ipc_message:
    /* Fast copy of IPC message struct (64 bytes)
     * r0 = source address
     * r1 = destination address
     * Uses NEON for single-cycle 16-byte transfers
     */
    vld1.64 {d0-d3}, [r0]!          /* Load 32 bytes */
    vld1.64 {d4-d7}, [r0]           /* Load 32 bytes */
    vst1.64 {d0-d3}, [r1]!          /* Store 32 bytes */
    vst1.64 {d4-d7}, [r1]           /* Store 32 bytes */
    bx lr

.global neon_zero_message_queue
neon_zero_message_queue:
    /* Fast zero 256-entry message queue (16KB)
     * r0 = base address
     * r1 = size in bytes
     */
    vmov.i8 q0, #0                  /* Zero vector */
    vmov.i8 q1, #0
    vmov.i8 q2, #0
    vmov.i8 q3, #0

neon_zero_loop:
    vst1.64 {d0-d3}, [r0]!          /* Store 32 bytes */
    vst1.64 {d4-d7}, [r0]!          /* Store 32 bytes */
    subs r1, r1, #64
    bgt neon_zero_loop
    bx lr

.global neon_copy_context
neon_copy_context:
    /* Fast copy CPU context (17 registers × 4 bytes = 68 bytes)
     * r0 = source context
     * r1 = destination context
     */
    vld1.64 {d0-d3}, [r0]!          /* Load 32 bytes */
    vld1.64 {d4-d7}, [r0]!          /* Load 32 bytes */
    vld1.32 {d8[0]}, [r0]           /* Load final 4 bytes */
    vst1.64 {d0-d3}, [r1]!          /* Store 32 bytes */
    vst1.64 {d4-d7}, [r1]!          /* Store 32 bytes */
    vst1.32 {d8[0]}, [r1]           /* Store final 4 bytes */
    bx lr

/* ========================================================================== */
/* OPTIMIZED SPINLOCK WITH BACKOFF                                           */
/* ========================================================================== */

.global spinlock_acquire_adaptive
spinlock_acquire_adaptive:
    /* Adaptive spinlock with exponential backoff
     * r0 = lock address
     * r1 = max retries before backoff
     * Returns: 0 on success, 1 on timeout
     */
    push {r4, r5}
    mov r4, #1                      /* Backoff delay counter */
    mov r5, r1                      /* Max retries */
    dmb sy

spinlock_adaptive_try:
    ldrex r1, [r0]
    cmp r1, #0
    bne spinlock_adaptive_backoff

    mov r1, #1
    strex r2, r1, [r0]
    cmp r2, #0
    beq spinlock_adaptive_success

spinlock_adaptive_backoff:
    subs r5, r5, #1
    beq spinlock_adaptive_timeout   /* Give up after max retries */

    /* Exponential backoff with WFE */
    mov r2, r4
spinlock_adaptive_delay:
    wfe
    subs r2, r2, #1
    bgt spinlock_adaptive_delay

    lsl r4, r4, #1                  /* Double backoff delay */
    cmp r4, #1024                   /* Cap at 1024 iterations */
    movgt r4, #1024
    b spinlock_adaptive_try

spinlock_adaptive_success:
    dmb sy
    mov r0, #0                      /* Success */
    pop {r4, r5}
    bx lr

spinlock_adaptive_timeout:
    mov r0, #1                      /* Timeout */
    pop {r4, r5}
    bx lr

/* ========================================================================== */
/* FAST CONTEXT SWITCH ASSEMBLY                                              */
/* ========================================================================== */

.global fast_context_switch
fast_context_switch:
    /* Ultra-fast context switch for same-priority processes
     * r0 = outgoing process context pointer
     * r1 = incoming process context pointer
     * Saves r0-r12, lr, sp, pc, cpsr (17 registers)
     */

    /* Save outgoing context using NEON for speed */
    stmia r0, {r0-r12, sp, lr}      /* Save r0-r12, sp, lr (15 regs) */
    str lr, [r0, #60]               /* Save PC (lr = return address) */
    mrs r2, cpsr
    str r2, [r0, #64]               /* Save CPSR */

    /* Memory barrier for SMP safety */
    dmb sy

    /* Restore incoming context */
    ldmia r1, {r0-r12, sp, lr}      /* Restore r0-r12, sp, lr */
    ldr pc, [r1, #60]               /* Jump to new PC */

/* ========================================================================== */
/* LOCK-FREE ATOMIC OPERATIONS (OPTIMIZED)                                   */
/* ========================================================================== */

.global atomic_inc_return
atomic_inc_return:
    /* Atomically increment and return new value
     * r0 = pointer to value
     * Returns: new value in r0
     */
    dmb sy
atomic_inc_loop:
    ldrex r1, [r0]
    add r1, r1, #1
    strex r2, r1, [r0]
    cmp r2, #0
    bne atomic_inc_loop
    dmb sy
    mov r0, r1
    bx lr

.global atomic_dec_return
atomic_dec_return:
    /* Atomically decrement and return new value */
    dmb sy
atomic_dec_loop:
    ldrex r1, [r0]
    sub r1, r1, #1
    strex r2, r1, [r0]
    cmp r2, #0
    bne atomic_dec_loop
    dmb sy
    mov r0, r1
    bx lr

.global atomic_swap
atomic_swap:
    /* Atomic swap (exchange)
     * r0 = pointer to value
     * r1 = new value
     * Returns: old value in r0
     */
    dmb sy
atomic_swap_loop:
    ldrex r2, [r0]
    strex r3, r1, [r0]
    cmp r3, #0
    bne atomic_swap_loop
    dmb sy
    mov r0, r2
    bx lr

/* ========================================================================== */
/* CPU CACHE MANAGEMENT FOR SMP                                              */
/* ========================================================================== */

.global flush_dcache_range
flush_dcache_range:
    /* Flush data cache for memory range
     * r0 = start address
     * r1 = end address
     */
    bic r0, r0, #63                 /* Align to cache line (64 bytes) */
flush_dcache_loop:
    mcr p15, 0, r0, c7, c14, 1      /* Clean and invalidate D-cache line */
    add r0, r0, #64
    cmp r0, r1
    blt flush_dcache_loop
    dsb sy
    bx lr

.global invalidate_icache
invalidate_icache:
    /* Invalidate entire instruction cache */
    mov r0, #0
    mcr p15, 0, r0, c7, c5, 0       /* Invalidate I-cache */
    dsb sy
    isb sy
    bx lr

.global flush_tlb_all
flush_tlb_all:
    /* Flush all TLB entries (for SMP safety after page table updates) */
    mov r0, #0
    mcr p15, 0, r0, c8, c7, 0       /* Invalidate entire TLB */
    dsb sy
    isb sy
    bx lr

/* ========================================================================== */
/* CPU HOTPLUG SUPPORT                                                        */
/* ========================================================================== */

.global cpu_online
cpu_online:
    /* Bring a CPU online
     * r0 = CPU ID (0-3)
     * Returns: 0 on success, -1 on error
     */
    push {r4-r6, lr}
    mov r4, r0                      /* Save CPU ID */

    /* Validate CPU ID */
    cmp r4, #MAX_CPUS
    movge r0, #-1
    bge cpu_online_exit

    /* Check if already online */
    ldr r5, =cpu_online_mask
    ldr r6, [r5]
    mov r1, #1
    lsl r1, r1, r4                  /* Create bit mask for this CPU */
    tst r6, r1
    movne r0, #-1                   /* Already online, fail */
    bne cpu_online_exit

    /* Mark CPU as online atomically */
cpu_online_set_mask:
    ldrex r2, [r5]
    orr r2, r2, r1
    strex r3, r2, [r5]
    cmp r3, #0
    bne cpu_online_set_mask

    dsb sy

    /* Send IPI to wake the CPU */
    mov r0, r4
    mov r1, #0                      /* Wake-up IPI */
    bl send_ipi

    mov r0, #0                      /* Success */

cpu_online_exit:
    pop {r4-r6, pc}

.global cpu_offline
cpu_offline:
    /* Take a CPU offline
     * r0 = CPU ID (0-3)
     * Returns: 0 on success, -1 on error
     */
    push {r4-r7, lr}
    mov r4, r0                      /* Save CPU ID */

    /* Cannot offline CPU 0 (boot CPU) */
    cmp r4, #0
    moveq r0, #-1
    beq cpu_offline_exit

    /* Validate CPU ID */
    cmp r4, #MAX_CPUS
    movge r0, #-1
    bge cpu_offline_exit

    /* Check if currently online */
    ldr r5, =cpu_online_mask
    ldr r6, [r5]
    mov r1, #1
    lsl r1, r1, r4                  /* Create bit mask */
    tst r6, r1
    moveq r0, #-1                   /* Not online, fail */
    beq cpu_offline_exit

    /* Mark CPU as offline atomically */
    mvn r7, r1                      /* Invert mask for clearing */
cpu_offline_clear_mask:
    ldrex r2, [r5]
    and r2, r2, r7
    strex r3, r2, [r5]
    cmp r3, #0
    bne cpu_offline_clear_mask

    dsb sy

    /* Send shutdown IPI */
    mov r0, r4
    mov r1, #3                      /* Shutdown IPI */
    bl send_ipi

    mov r0, #0                      /* Success */

cpu_offline_exit:
    pop {r4-r7, pc}

.global cpu_is_online
cpu_is_online:
    /* Check if CPU is online
     * r0 = CPU ID
     * Returns: 1 if online, 0 if offline
     */
    cmp r0, #MAX_CPUS
    movge r0, #0
    bxge lr

    ldr r1, =cpu_online_mask
    ldr r1, [r1]
    mov r2, #1
    lsl r2, r2, r0
    and r0, r1, r2
    cmp r0, #0
    movne r0, #1
    bx lr

.global get_online_cpu_count
get_online_cpu_count:
    /* Returns: number of online CPUs */
    ldr r0, =cpu_online_mask
    ldr r0, [r0]

    /* Count bits set (population count) */
    mov r1, #0
cpu_count_loop:
    tst r0, #1
    addne r1, r1, #1
    lsr r0, r0, #1
    cmp r0, #0
    bne cpu_count_loop

    mov r0, r1
    bx lr

/* ========================================================================== */
/* PRIORITY INHERITANCE FOR IPC                                              */
/* ========================================================================== */

.global ipc_acquire_with_priority_boost
ipc_acquire_with_priority_boost:
    /* Acquire IPC lock with priority inheritance
     * r0 = lock address
     * r1 = current priority
     * r2 = lock owner's priority pointer
     * Returns: 0 on success
     */
    push {r4-r6, lr}
    mov r4, r0                      /* Save lock address */
    mov r5, r1                      /* Save current priority */
    mov r6, r2                      /* Save owner priority pointer */

    dmb sy

ipc_pi_acquire_loop:
    ldrex r1, [r4]
    cmp r1, #0
    beq ipc_pi_try_acquire

    /* Lock is held, check if we need to boost owner's priority */
    ldr r3, [r6]                    /* Load owner's priority */
    cmp r5, r3                      /* Compare with our priority */
    blt ipc_pi_boost                /* Boost if we're higher priority */

    wfe
    b ipc_pi_acquire_loop

ipc_pi_boost:
    /* Temporarily boost lock owner's priority */
    str r5, [r6]                    /* Set owner priority = our priority */
    dsb sy
    sev                             /* Wake owner to run at higher priority */
    wfe
    b ipc_pi_acquire_loop

ipc_pi_try_acquire:
    mov r1, #1
    strex r2, r1, [r4]
    cmp r2, #0
    bne ipc_pi_acquire_loop

    dmb sy
    mov r0, #0
    pop {r4-r6, pc}

.global ipc_release_with_priority_restore
ipc_release_with_priority_restore:
    /* Release IPC lock and restore original priority
     * r0 = lock address
     * r1 = original priority
     * r2 = current priority pointer
     */
    dmb sy

    /* Restore original priority */
    str r1, [r2]

    /* Release lock */
    mov r1, #0
    str r1, [r0]

    dsb sy
    sev                             /* Wake waiting processes */
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
