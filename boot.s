/*
 * ============================================================================
 * ARM ASSEMBLY BOOTSTRAP FOR MINIX-STYLE MICROKERNEL
 * ============================================================================
 * Target: ARMv7-A with NEON extensions
 * Bootloader: Minimal bare-metal init for QEMU virt machine
 * Features: Vector table, context switching, UART, NEON operations
 *
 * Literate Assembly - Full descriptive labels with code-golfed implementation
 * ============================================================================
 */

.syntax unified
.cpu cortex-a15
.fpu neon-vfpv4
.arm

/* ========================================================================== */
/* GLOBAL SYMBOLS AND CONSTANTS                                               */
/* ========================================================================== */

.equ STACK_SIZE_PER_MODE, 0x1000
.equ UART0_BASE_ADDRESS, 0x09000000          /* QEMU virt UART */
.equ UART0_DATA_REGISTER, 0x09000000
.equ UART0_FLAG_REGISTER, 0x09000018
.equ UART_TRANSMIT_FIFO_FULL_BIT, 0x20

/* ARM Processor Modes */
.equ ARM_MODE_USER, 0x10
.equ ARM_MODE_FIQ, 0x11
.equ ARM_MODE_IRQ, 0x12
.equ ARM_MODE_SUPERVISOR, 0x13
.equ ARM_MODE_ABORT, 0x17
.equ ARM_MODE_UNDEFINED, 0x1B
.equ ARM_MODE_SYSTEM, 0x1F

/* Status Register Bits */
.equ ARM_INTERRUPT_DISABLE_IRQ, 0x80
.equ ARM_INTERRUPT_DISABLE_FIQ, 0x40

/* ========================================================================== */
/* INTERRUPT VECTOR TABLE (ARM Standard Layout)                              */
/* ========================================================================== */

.section .vectors, "ax"
.global arm_exception_vector_table_base
arm_exception_vector_table_base:
    ldr pc, =reset_handler_initialization_entry_point
    ldr pc, =undefined_instruction_exception_handler
    ldr pc, =supervisor_call_software_interrupt_handler
    ldr pc, =prefetch_abort_exception_handler
    ldr pc, =data_abort_exception_handler
    nop                                      /* Reserved vector */
    ldr pc, =hardware_interrupt_request_handler
    ldr pc, =fast_interrupt_request_handler

/* ========================================================================== */
/* BOOT CODE - HARDWARE INITIALIZATION                                        */
/* ========================================================================== */

.section .text
.global reset_handler_initialization_entry_point
reset_handler_initialization_entry_point:
    /* Disable interrupts during initialization */
    cpsid if

    /* Initialize stack pointers for all ARM processor modes */
    msr cpsr_c, #(ARM_MODE_FIQ | ARM_INTERRUPT_DISABLE_IRQ | ARM_INTERRUPT_DISABLE_FIQ)
    ldr sp, =fast_interrupt_mode_stack_pointer_initial

    msr cpsr_c, #(ARM_MODE_IRQ | ARM_INTERRUPT_DISABLE_IRQ | ARM_INTERRUPT_DISABLE_FIQ)
    ldr sp, =interrupt_request_mode_stack_pointer_initial

    msr cpsr_c, #(ARM_MODE_ABORT | ARM_INTERRUPT_DISABLE_IRQ | ARM_INTERRUPT_DISABLE_FIQ)
    ldr sp, =abort_mode_stack_pointer_initial

    msr cpsr_c, #(ARM_MODE_UNDEFINED | ARM_INTERRUPT_DISABLE_IRQ | ARM_INTERRUPT_DISABLE_FIQ)
    ldr sp, =undefined_mode_stack_pointer_initial

    msr cpsr_c, #(ARM_MODE_SUPERVISOR | ARM_INTERRUPT_DISABLE_IRQ | ARM_INTERRUPT_DISABLE_FIQ)
    ldr sp, =supervisor_mode_stack_pointer_initial

    /* Clear BSS section to zero */
    ldr r0, =__bss_start__
    ldr r1, =__bss_end__
    mov r2, #0
clear_bss_section_loop:
    cmp r0, r1
    strlt r2, [r0], #4
    blt clear_bss_section_loop

    /* Enable NEON/VFP coprocessor access */
    mrc p15, 0, r0, c1, c0, 2               /* Read CPACR */
    orr r0, r0, #(0xF << 20)                /* Enable CP10 & CP11 (VFP/NEON) */
    mcr p15, 0, r0, c1, c0, 2               /* Write CPACR */
    isb
    mov r0, #0x40000000                      /* Enable VFP */
    vmsr fpexc, r0

    /* Install exception vector table */
    ldr r0, =arm_exception_vector_table_base
    mcr p15, 0, r0, c12, c0, 0              /* Set VBAR */

    /* Jump to Ada microkernel main */
    bl _ada_microkernel_operating_system_executive

    /* Infinite loop if kernel returns */
kernel_panic_infinite_halt_loop:
    wfi
    b kernel_panic_infinite_halt_loop

/* ========================================================================== */
/* EXCEPTION HANDLERS                                                         */
/* ========================================================================== */

undefined_instruction_exception_handler:
    /* Save context */
    sub lr, lr, #4
    srsdb sp!, #ARM_MODE_SUPERVISOR
    push {r0-r12, lr}

    /* Handle undefined instruction */
    mov r0, #1                               /* Exception type */
    bl kernel_exception_dispatcher

    /* Restore context */
    pop {r0-r12, lr}
    rfeia sp!

supervisor_call_software_interrupt_handler:
    /* System call entry point from user mode */
    push {r0-r12, lr}

    /* r0 = syscall number, r1-r3 = parameters */
    /* Result returned in r0 */
    bl handle_system_call_from_user_mode

    pop {r1-r12, lr}                         /* Don't pop r0 (return value) */
    movs pc, lr

prefetch_abort_exception_handler:
    sub lr, lr, #4
    srsdb sp!, #ARM_MODE_SUPERVISOR
    push {r0-r12, lr}
    mov r0, #2
    bl kernel_exception_dispatcher
    pop {r0-r12, lr}
    rfeia sp!

data_abort_exception_handler:
    sub lr, lr, #8
    srsdb sp!, #ARM_MODE_SUPERVISOR
    push {r0-r12, lr}
    mov r0, #3
    bl kernel_exception_dispatcher
    pop {r0-r12, lr}
    rfeia sp!

hardware_interrupt_request_handler:
    /* Save minimal context for IRQ */
    sub lr, lr, #4
    srsdb sp!, #ARM_MODE_SUPERVISOR
    push {r0-r3, r12, lr}

    /* Call C interrupt handler */
    bl process_hardware_interrupt_controller

    /* Restore and return */
    pop {r0-r3, r12, lr}
    rfeia sp!

fast_interrupt_request_handler:
    /* Fast path for critical interrupts */
    subs pc, lr, #4

/* ========================================================================== */
/* CONTEXT SWITCHING (Core scheduler primitive)                               */
/* ========================================================================== */

.global context_switch
context_switch:
    /* r0 = target process ID */
    /* Save current process context */
    push {r0-r12, lr}

    /* Load new process context from process table */
    /* Implementation simplified - would load from PCB array */
    ldr r1, =current_process_context_save_area
    add r1, r1, r0, lsl #6                   /* 64 bytes per context */
    ldmia r1, {r0-r12, lr}

    /* Return to new process */
    bx lr

/* ========================================================================== */
/* UART DRIVER FOR DEBUG OUTPUT                                              */
/* ========================================================================== */

.global uart_putc
uart_putc:
    /* r0 = character to output */
uart_wait_for_transmit_ready:
    ldr r1, =UART0_FLAG_REGISTER
    ldr r2, [r1]
    tst r2, #UART_TRANSMIT_FIFO_FULL_BIT
    bne uart_wait_for_transmit_ready

    ldr r1, =UART0_DATA_REGISTER
    strb r0, [r1]
    bx lr

.global uart_puts
uart_puts:
    /* r0 = pointer to null-terminated string */
    push {r4, lr}
    mov r4, r0
uart_puts_loop:
    ldrb r0, [r4], #1
    cmp r0, #0
    beq uart_puts_done
    bl uart_putc
    b uart_puts_loop
uart_puts_done:
    pop {r4, pc}

/* ========================================================================== */
/* NEON-ACCELERATED MEMORY OPERATIONS                                         */
/* ========================================================================== */

.global neon_memcpy
neon_memcpy:
    /* r0 = source, r1 = destination, r2 = byte count */
    /* Use NEON quad-word (128-bit) transfers for speed */
    push {r4, lr}

    /* Handle alignment and small copies */
    cmp r2, #64
    blt neon_memcpy_byte_fallback

    /* Align to 16 bytes */
    ands r3, r1, #15
    beq neon_memcpy_aligned
    rsb r3, r3, #16
    sub r2, r2, r3
neon_memcpy_align_loop:
    ldrb r4, [r0], #1
    strb r4, [r1], #1
    subs r3, r3, #1
    bne neon_memcpy_align_loop

neon_memcpy_aligned:
    /* Copy 64 bytes at a time using NEON */
    subs r2, r2, #64
    blt neon_memcpy_cleanup
neon_memcpy_neon_loop:
    vld1.8 {q0, q1}, [r0]!                   /* Load 32 bytes */
    vld1.8 {q2, q3}, [r0]!                   /* Load 32 bytes */
    vst1.8 {q0, q1}, [r1]!                   /* Store 32 bytes */
    vst1.8 {q2, q3}, [r1]!                   /* Store 32 bytes */
    subs r2, r2, #64
    bge neon_memcpy_neon_loop

neon_memcpy_cleanup:
    adds r2, r2, #64
    beq neon_memcpy_done

neon_memcpy_byte_fallback:
    ldrb r3, [r0], #1
    strb r3, [r1], #1
    subs r2, r2, #1
    bne neon_memcpy_byte_fallback

neon_memcpy_done:
    pop {r4, pc}

/* ========================================================================== */
/* INTERRUPT CONTROL                                                          */
/* ========================================================================== */

.global enable_interrupts
enable_interrupts:
    cpsie i
    bx lr

.global disable_interrupts
disable_interrupts:
    cpsid i
    bx lr

.global init_vectors
init_vectors:
    /* Vector table already installed in reset handler */
    bx lr

/* ========================================================================== */
/* STUB HANDLERS (Minimal implementations)                                    */
/* ========================================================================== */

kernel_exception_dispatcher:
    /* r0 = exception type */
    /* Minimal panic - halt system */
    b kernel_panic_infinite_halt_loop

process_hardware_interrupt_controller:
    /* Acknowledge and process interrupts */
    /* Implementation would query GIC for interrupt number */
    bx lr

handle_system_call_from_user_mode:
    /* Forward to Ada handler */
    /* In full implementation, would call Handle_System_Call_Interrupt */
    mov r0, #0                               /* Success */
    bx lr

/* ========================================================================== */
/* DATA SECTION - STACK ALLOCATION                                            */
/* ========================================================================== */

.section .bss
.align 4

fast_interrupt_mode_stack_space:
    .space STACK_SIZE_PER_MODE
fast_interrupt_mode_stack_pointer_initial:

interrupt_request_mode_stack_space:
    .space STACK_SIZE_PER_MODE
interrupt_request_mode_stack_pointer_initial:

abort_mode_stack_space:
    .space STACK_SIZE_PER_MODE
abort_mode_stack_pointer_initial:

undefined_mode_stack_space:
    .space STACK_SIZE_PER_MODE
undefined_mode_stack_pointer_initial:

supervisor_mode_stack_space:
    .space STACK_SIZE_PER_MODE * 4
supervisor_mode_stack_pointer_initial:

current_process_context_save_area:
    .space 4096                              /* 64 processes × 64 bytes */

.section .data
boot_banner_string:
    .asciz "ARM Boot Complete\n"

.end
