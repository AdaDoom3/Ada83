	.text
	.file	"report.ll"
	.globl	__ada_setjmp                    # -- Begin function __ada_setjmp
	.p2align	4, 0x90
	.type	__ada_setjmp,@function
__ada_setjmp:                           # @__ada_setjmp
	.cfi_startproc
# %bb.0:
	pushq	%rax
	.cfi_def_cfa_offset 16
	movl	$200, %edi
	callq	malloc@PLT
	popq	%rcx
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end0:
	.size	__ada_setjmp, .Lfunc_end0-__ada_setjmp
	.cfi_endproc
                                        # -- End function
	.globl	__ada_raise                     # -- Begin function __ada_raise
	.p2align	4, 0x90
	.type	__ada_raise,@function
__ada_raise:                            # @__ada_raise
	.cfi_startproc
# %bb.0:
	pushq	%rax
	.cfi_def_cfa_offset 16
	movq	__ex_cur@GOTPCREL(%rip), %rax
	movq	%rdi, (%rax)
	movq	__eh_cur@GOTPCREL(%rip), %rdi
	movl	$1, %esi
	callq	longjmp@PLT
	popq	%rax
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end1:
	.size	__ada_raise, .Lfunc_end1-__ada_raise
	.cfi_endproc
                                        # -- End function
	.globl	__ada_delay                     # -- Begin function __ada_delay
	.p2align	4, 0x90
	.type	__ada_delay,@function
__ada_delay:                            # @__ada_delay
	.cfi_startproc
# %bb.0:
	pushq	%rax
	.cfi_def_cfa_offset 16
                                        # kill: def $edi killed $edi killed $rdi
	callq	usleep@PLT
	popq	%rax
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end2:
	.size	__ada_delay, .Lfunc_end2-__ada_delay
	.cfi_endproc
                                        # -- End function
	.globl	__ada_powi                      # -- Begin function __ada_powi
	.p2align	4, 0x90
	.type	__ada_powi,@function
__ada_powi:                             # @__ada_powi
	.cfi_startproc
# %bb.0:
	testq	%rsi, %rsi
	js	.LBB3_6
# %bb.1:                                # %s.preheader
	movl	$1, %eax
	jmp	.LBB3_2
	.p2align	4, 0x90
.LBB3_4:                                # %k
                                        #   in Loop: Header=BB3_2 Depth=1
	shrq	%rsi
	imulq	%rdi, %rdi
	testq	%rsi, %rsi
	je	.LBB3_5
.LBB3_2:                                # %s
                                        # =>This Inner Loop Header: Depth=1
	testb	$1, %sil
	je	.LBB3_4
# %bb.3:                                # %m
                                        #   in Loop: Header=BB3_2 Depth=1
	imulq	%rdi, %rax
	jmp	.LBB3_4
.LBB3_5:                                # %o
	retq
.LBB3_6:                                # %e
	xorl	%eax, %eax
	retq
.Lfunc_end3:
	.size	__ada_powi, .Lfunc_end3-__ada_powi
	.cfi_endproc
                                        # -- End function
	.globl	__text_io_put_i64               # -- Begin function __text_io_put_i64
	.p2align	4, 0x90
	.type	__text_io_put_i64,@function
__text_io_put_i64:                      # @__text_io_put_i64
	.cfi_startproc
# %bb.0:
	pushq	%rbx
	.cfi_def_cfa_offset 16
	subq	$32, %rsp
	.cfi_def_cfa_offset 48
	.cfi_offset %rbx, -16
	movq	%rdi, %rdx
	movq	.fmt_i64@GOTPCREL(%rip), %rsi
	movq	%rsp, %rbx
	movq	%rbx, %rdi
	xorl	%eax, %eax
	callq	sprintf@PLT
	movq	%rbx, %rdi
	callq	puts@PLT
	addq	$32, %rsp
	.cfi_def_cfa_offset 16
	popq	%rbx
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end4:
	.size	__text_io_put_i64, .Lfunc_end4-__text_io_put_i64
	.cfi_endproc
                                        # -- End function
	.globl	__text_io_put_f64               # -- Begin function __text_io_put_f64
	.p2align	4, 0x90
	.type	__text_io_put_f64,@function
__text_io_put_f64:                      # @__text_io_put_f64
	.cfi_startproc
# %bb.0:
	pushq	%rbx
	.cfi_def_cfa_offset 16
	subq	$32, %rsp
	.cfi_def_cfa_offset 48
	.cfi_offset %rbx, -16
	movq	.fmt_f64@GOTPCREL(%rip), %rsi
	movq	%rsp, %rbx
	movq	%rbx, %rdi
	movb	$1, %al
	callq	sprintf@PLT
	movq	%rbx, %rdi
	callq	puts@PLT
	addq	$32, %rsp
	.cfi_def_cfa_offset 16
	popq	%rbx
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end5:
	.size	__text_io_put_f64, .Lfunc_end5-__text_io_put_f64
	.cfi_endproc
                                        # -- End function
	.globl	__text_io_put_str               # -- Begin function __text_io_put_str
	.p2align	4, 0x90
	.type	__text_io_put_str,@function
__text_io_put_str:                      # @__text_io_put_str
	.cfi_startproc
# %bb.0:
	pushq	%rax
	.cfi_def_cfa_offset 16
	callq	puts@PLT
	popq	%rax
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end6:
	.size	__text_io_put_str, .Lfunc_end6-__text_io_put_str
	.cfi_endproc
                                        # -- End function
	.globl	__text_io_newline               # -- Begin function __text_io_newline
	.p2align	4, 0x90
	.type	__text_io_newline,@function
__text_io_newline:                      # @__text_io_newline
	.cfi_startproc
# %bb.0:
	pushq	%rax
	.cfi_def_cfa_offset 16
	movw	$10, 6(%rsp)
	leaq	6(%rsp), %rdi
	callq	puts@PLT
	popq	%rax
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end7:
	.size	__text_io_newline, .Lfunc_end7-__text_io_newline
	.cfi_endproc
                                        # -- End function
	.globl	REPORT__TEST                    # -- Begin function REPORT__TEST
	.p2align	4, 0x90
	.type	REPORT__TEST,@function
REPORT__TEST:                           # @REPORT__TEST
	.cfi_startproc
# %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	pushq	%r15
	.cfi_def_cfa_offset 24
	pushq	%r14
	.cfi_def_cfa_offset 32
	pushq	%r13
	.cfi_def_cfa_offset 40
	pushq	%r12
	.cfi_def_cfa_offset 48
	pushq	%rbx
	.cfi_def_cfa_offset 56
	subq	$440, %rsp                      # imm = 0x1B8
	.cfi_def_cfa_offset 496
	.cfi_offset %rbx, -56
	.cfi_offset %r12, -48
	.cfi_offset %r13, -40
	.cfi_offset %r14, -32
	.cfi_offset %r15, -24
	.cfi_offset %rbp, -16
	movq	%rdi, %r14
	movq	%rdi, 24(%rsp)
	movq	%rsi, 16(%rsp)
	movl	$1414743380, 10(%rsp)           # imm = 0x54534554
	movw	$32, 14(%rsp)
	leaq	10(%rsp), %r15
	movq	%r15, %rdi
	callq	strlen@PLT
	movq	%rax, %r12
	movq	%r14, %rdi
	callq	strlen@PLT
	movq	%rax, %r13
	leaq	(%r12,%rax), %rbp
	leaq	1(%r12,%rax), %rdi
	callq	malloc@PLT
	movq	%rax, %rbx
	movq	%rax, %rdi
	movq	%r15, %rsi
	movq	%r12, %rdx
	callq	memcpy@PLT
	addq	%rbx, %r12
	movq	%r12, %rdi
	movq	%r14, %rsi
	movq	%r13, %rdx
	callq	memcpy@PLT
	movb	$0, (%rbx,%rbp)
	movb	$0, 9(%rsp)
	movw	$8250, 7(%rsp)                  # imm = 0x203A
	movq	%rbx, %rdi
	callq	strlen@PLT
	movq	%rax, %r15
	leaq	7(%rsp), %r12
	movq	%r12, %rdi
	callq	strlen@PLT
	movq	%rax, %r13
	leaq	(%r15,%rax), %rbp
	leaq	1(%r15,%rax), %rdi
	callq	malloc@PLT
	movq	%rax, %r14
	movq	%rax, %rdi
	movq	%rbx, %rsi
	movq	%r15, %rdx
	callq	memcpy@PLT
	addq	%r14, %r15
	movq	%r15, %rdi
	movq	%r12, %rsi
	movq	%r13, %rdx
	callq	memcpy@PLT
	movb	$0, (%r14,%rbp)
	movq	16(%rsp), %rbx
	movq	%r14, %rdi
	callq	strlen@PLT
	movq	%rax, %r15
	movq	%rbx, %rdi
	callq	strlen@PLT
	movq	%rax, %r12
	leaq	(%r15,%rax), %rbp
	leaq	1(%r15,%rax), %rdi
	callq	malloc@PLT
	movq	%rax, %r13
	movq	%rax, %rdi
	movq	%r14, %rsi
	movq	%r15, %rdx
	callq	memcpy@PLT
	addq	%r13, %r15
	movq	%r15, %rdi
	movq	%rbx, %rsi
	movq	%r12, %rdx
	callq	memcpy@PLT
	movb	$0, (%r13,%rbp)
	movq	%r13, %rdi
	callq	__text_io_put_str@PLT
	callq	__text_io_newline@PLT
	movq	REPORT__F@GOTPCREL(%rip), %rax
	movq	$0, (%rax)
	addq	$440, %rsp                      # imm = 0x1B8
	.cfi_def_cfa_offset 56
	popq	%rbx
	.cfi_def_cfa_offset 48
	popq	%r12
	.cfi_def_cfa_offset 40
	popq	%r13
	.cfi_def_cfa_offset 32
	popq	%r14
	.cfi_def_cfa_offset 24
	popq	%r15
	.cfi_def_cfa_offset 16
	popq	%rbp
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end8:
	.size	REPORT__TEST, .Lfunc_end8-REPORT__TEST
	.cfi_endproc
                                        # -- End function
	.globl	REPORT__FAILED                  # -- Begin function REPORT__FAILED
	.p2align	4, 0x90
	.type	REPORT__FAILED,@function
REPORT__FAILED:                         # @REPORT__FAILED
	.cfi_startproc
# %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	pushq	%r15
	.cfi_def_cfa_offset 24
	pushq	%r14
	.cfi_def_cfa_offset 32
	pushq	%r13
	.cfi_def_cfa_offset 40
	pushq	%r12
	.cfi_def_cfa_offset 48
	pushq	%rbx
	.cfi_def_cfa_offset 56
	subq	$440, %rsp                      # imm = 0x1B8
	.cfi_def_cfa_offset 496
	.cfi_offset %rbx, -56
	.cfi_offset %r12, -48
	.cfi_offset %r13, -40
	.cfi_offset %r14, -32
	.cfi_offset %r15, -24
	.cfi_offset %rbp, -16
	movq	%rdi, %rbx
	movq	%rdi, 24(%rsp)
	movabsq	$2322243622286213446, %rax      # imm = 0x203A44454C494146
	movq	%rax, 15(%rsp)
	movb	$0, 23(%rsp)
	leaq	15(%rsp), %r14
	movq	%r14, %rdi
	callq	strlen@PLT
	movq	%rax, %r15
	movq	%rbx, %rdi
	callq	strlen@PLT
	movq	%rax, %r12
	leaq	(%r15,%rax), %rbp
	leaq	1(%r15,%rax), %rdi
	callq	malloc@PLT
	movq	%rax, %r13
	movq	%rax, %rdi
	movq	%r14, %rsi
	movq	%r15, %rdx
	callq	memcpy@PLT
	addq	%r13, %r15
	movq	%r15, %rdi
	movq	%rbx, %rsi
	movq	%r12, %rdx
	callq	memcpy@PLT
	movb	$0, (%r13,%rbp)
	movq	%r13, %rdi
	callq	__text_io_put_str@PLT
	callq	__text_io_newline@PLT
	movq	REPORT__F@GOTPCREL(%rip), %rax
	movq	$1, (%rax)
	addq	$440, %rsp                      # imm = 0x1B8
	.cfi_def_cfa_offset 56
	popq	%rbx
	.cfi_def_cfa_offset 48
	popq	%r12
	.cfi_def_cfa_offset 40
	popq	%r13
	.cfi_def_cfa_offset 32
	popq	%r14
	.cfi_def_cfa_offset 24
	popq	%r15
	.cfi_def_cfa_offset 16
	popq	%rbp
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end9:
	.size	REPORT__FAILED, .Lfunc_end9-REPORT__FAILED
	.cfi_endproc
                                        # -- End function
	.globl	REPORT__RESULT                  # -- Begin function REPORT__RESULT
	.p2align	4, 0x90
	.type	REPORT__RESULT,@function
REPORT__RESULT:                         # @REPORT__RESULT
	.cfi_startproc
# %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$416, %rsp                      # imm = 0x1A0
	movq	REPORT__F@GOTPCREL(%rip), %rax
	cmpq	$0, (%rax)
	jne	.LBB10_2
# %bb.1:                                # %L0
	movq	%rsp, %rax
	leaq	-16(%rax), %rdi
	movq	%rdi, %rsp
	movb	$0, -10(%rax)
	movw	$17477, -12(%rax)               # imm = 0x4445
	movl	$1397965136, -16(%rax)          # imm = 0x53534150
	jmp	.LBB10_3
.LBB10_2:                               # %L1
	movq	%rsp, %rax
	leaq	-16(%rax), %rdi
	movq	%rdi, %rsp
	movb	$0, -10(%rax)
	movw	$17477, -12(%rax)               # imm = 0x4445
	movl	$1279869254, -16(%rax)          # imm = 0x4C494146
.LBB10_3:                               # %L2
	callq	__text_io_put_str@PLT
	callq	__text_io_newline@PLT
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end10:
	.size	REPORT__RESULT, .Lfunc_end10-REPORT__RESULT
	.cfi_endproc
                                        # -- End function
	.globl	REPORT__IDENT_INT               # -- Begin function REPORT__IDENT_INT
	.p2align	4, 0x90
	.type	REPORT__IDENT_INT,@function
REPORT__IDENT_INT:                      # @REPORT__IDENT_INT
	.cfi_startproc
# %bb.0:
	movq	%rdi, %rax
	movq	%rdi, -8(%rsp)
	retq
.Lfunc_end11:
	.size	REPORT__IDENT_INT, .Lfunc_end11-REPORT__IDENT_INT
	.cfi_endproc
                                        # -- End function
	.globl	REPORT__IDENT_BOOL              # -- Begin function REPORT__IDENT_BOOL
	.p2align	4, 0x90
	.type	REPORT__IDENT_BOOL,@function
REPORT__IDENT_BOOL:                     # @REPORT__IDENT_BOOL
	.cfi_startproc
# %bb.0:
	movq	%rdi, %rax
	movq	%rdi, -8(%rsp)
	retq
.Lfunc_end12:
	.size	REPORT__IDENT_BOOL, .Lfunc_end12-REPORT__IDENT_BOOL
	.cfi_endproc
                                        # -- End function
	.globl	REPORT__IDENT_CHAR              # -- Begin function REPORT__IDENT_CHAR
	.p2align	4, 0x90
	.type	REPORT__IDENT_CHAR,@function
REPORT__IDENT_CHAR:                     # @REPORT__IDENT_CHAR
	.cfi_startproc
# %bb.0:
	movq	%rdi, %rax
	movq	%rdi, -8(%rsp)
	retq
.Lfunc_end13:
	.size	REPORT__IDENT_CHAR, .Lfunc_end13-REPORT__IDENT_CHAR
	.cfi_endproc
                                        # -- End function
	.globl	REPORT__IDENT_STR               # -- Begin function REPORT__IDENT_STR
	.p2align	4, 0x90
	.type	REPORT__IDENT_STR,@function
REPORT__IDENT_STR:                      # @REPORT__IDENT_STR
	.cfi_startproc
# %bb.0:
	movq	%rdi, %rax
	movq	%rdi, -8(%rsp)
	retq
.Lfunc_end14:
	.size	REPORT__IDENT_STR, .Lfunc_end14-REPORT__IDENT_STR
	.cfi_endproc
                                        # -- End function
	.type	__eh_cur,@object                # @__eh_cur
	.bss
	.globl	__eh_cur
	.p2align	3, 0x0
__eh_cur:
	.quad	0
	.size	__eh_cur, 8

	.type	__ex_cur,@object                # @__ex_cur
	.globl	__ex_cur
	.p2align	3, 0x0
__ex_cur:
	.quad	0
	.size	__ex_cur, 8

	.type	.ex.CONSTRAINT_ERROR,@object    # @.ex.CONSTRAINT_ERROR
	.section	.rodata,"a",@progbits
	.globl	.ex.CONSTRAINT_ERROR
	.p2align	4, 0x0
.ex.CONSTRAINT_ERROR:
	.asciz	"CONSTRAINT_ERROR"
	.size	.ex.CONSTRAINT_ERROR, 17

	.type	.ex.PROGRAM_ERROR,@object       # @.ex.PROGRAM_ERROR
	.globl	.ex.PROGRAM_ERROR
.ex.PROGRAM_ERROR:
	.asciz	"PROGRAM_ERROR"
	.size	.ex.PROGRAM_ERROR, 14

	.type	.ex.STORAGE_ERROR,@object       # @.ex.STORAGE_ERROR
	.globl	.ex.STORAGE_ERROR
.ex.STORAGE_ERROR:
	.asciz	"STORAGE_ERROR"
	.size	.ex.STORAGE_ERROR, 14

	.type	.ex.TASKING_ERROR,@object       # @.ex.TASKING_ERROR
	.globl	.ex.TASKING_ERROR
.ex.TASKING_ERROR:
	.asciz	"TASKING_ERROR"
	.size	.ex.TASKING_ERROR, 14

	.type	.fmt_i64,@object                # @.fmt_i64
	.globl	.fmt_i64
.fmt_i64:
	.asciz	"%lld"
	.size	.fmt_i64, 5

	.type	.fmt_f64,@object                # @.fmt_f64
	.globl	.fmt_f64
.fmt_f64:
	.asciz	"%g"
	.size	.fmt_f64, 3

	.type	REPORT__F,@object               # @REPORT__F
	.bss
	.globl	REPORT__F
	.p2align	3, 0x0
REPORT__F:
	.quad	0                               # 0x0
	.size	REPORT__F, 8

	.section	".note.GNU-stack","",@progbits
