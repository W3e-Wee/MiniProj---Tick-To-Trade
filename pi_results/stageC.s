	.arch armv8-a
	.file	"stageC.c"
	.text
	.align	2
	.p2align 5,,15
	.type	now_ns, %function
now_ns:
.LFB22:
	.cfi_startproc
	stp	x29, x30, [sp, -32]!
	.cfi_def_cfa_offset 32
	.cfi_offset 29, -32
	.cfi_offset 30, -24
	mov	w0, 1
	mov	x29, sp
	add	x1, sp, 16
	bl	clock_gettime
	ldp	x2, x1, [sp, 16]
	mov	x0, 51712
	movk	x0, 0x3b9a, lsl 16
	ldp	x29, x30, [sp], 32
	.cfi_restore 30
	.cfi_restore 29
	.cfi_def_cfa_offset 0
	madd	x0, x2, x0, x1
	ret
	.cfi_endproc
.LFE22:
	.size	now_ns, .-now_ns
	.section	.rodata.str1.8,"aMS",@progbits,1
	.align	3
.LC0:
	.string	"data/stress_full.csv"
	.align	3
.LC1:
	.string	"Error: Unable to open %s\n"
	.align	3
.LC2:
	.string	"loaded %d trades, replaying at %dx\n"
	.align	3
.LC3:
	.string	"Timer call cost: about %.0f ns\n"
	.align	3
.LC4:
	.string	"%5.0f s elapsed: %d trades released, market time %.1f s\n"
	.align	3
.LC5:
	.string	"old fixed array would reserve: %lld bytes\n"
	.align	3
.LC6:
	.string	"malloc reserved:               %lld bytes\n"
	.section	.text.startup,"ax",@progbits
	.align	2
	.p2align 5,,15
	.global	main
	.type	main, %function
main:
.LFB29:
	.cfi_startproc
	movi	v31.4s, 0
	stp	x29, x30, [sp, -192]!
	.cfi_def_cfa_offset 192
	.cfi_offset 29, -192
	.cfi_offset 30, -184
	mov	x29, sp
	stp	x21, x22, [sp, 32]
	.cfi_offset 21, -160
	.cfi_offset 22, -152
	add	x22, sp, 128
	stp	x19, x20, [sp, 16]
	.cfi_offset 19, -176
	.cfi_offset 20, -168
	adrp	x19, .LC0
	add	x19, x19, :lo12:.LC0
	mov	x0, x19
	stp	q31, q31, [x22]
	str	q31, [x22, 32]
	str	xzr, [x22, 48]
	bl	load_trades
	cbz	w0, .L15
	stp	x23, x24, [sp, 48]
	.cfi_offset 24, -136
	.cfi_offset 23, -144
	mov	w2, 1000
	adrp	x0, .LC2
	adrp	x23, :got:n_trades;ldr	x23, [x23, :got_lo12:n_trades]
	stp	x25, x26, [sp, 64]
	.cfi_offset 26, -120
	.cfi_offset 25, -128
	add	x0, x0, :lo12:.LC2
	add	x20, sp, 112
	mov	w19, 1000
	ldr	w1, [x23]
	bl	printf
	bl	now_ns
	mov	x21, x0
	.p2align 5,,15
.L7:
	mov	x1, x20
	mov	w0, 1
	bl	clock_gettime
	ldp	x1, x2, [sp, 112]
	subs	w19, w19, #1
	bne	.L7
	mov	x24, 51712
	mov	x3, 70368744177664
	movk	x24, 0x3b9a, lsl 16
	movk	x3, 0x408f, lsl 48
	fmov	d0, x3
	adrp	x0, .LC3
	madd	x1, x1, x24, x2
	add	x0, x0, :lo12:.LC3
	sub	x1, x1, x21
	ucvtf	d31, x1
	fdiv	d0, d31, d0
	bl	printf
	adrp	x0, :got:trades;ldr	x0, [x0, :got_lo12:trades]
	ldr	x0, [x0]
	ldr	x0, [x0]
	stp	x0, x0, [sp, 144]
	bl	now_ns
	mov	x25, x0
	ldr	w1, [x23]
	cmp	w1, 0
	ble	.L8
	mov	x26, 58368
	mov	x1, 145685290680320
	movk	x26, 0x540b, lsl 16
	mov	x0, 225833675390976
	movk	x1, 0x412e, lsl 48
	movk	x0, 0x41cd, lsl 48
	mov	w19, 0
	movk	x26, 0x2, lsl 32
	stp	x27, x28, [sp, 80]
	.cfi_offset 28, -104
	.cfi_offset 27, -112
	stp	d14, d15, [sp, 96]
	.cfi_offset 79, -88
	.cfi_offset 78, -96
	fmov	d14, x1
	fmov	d15, x0
	b	.L10
	.p2align 2,,3
.L9:
	ldr	w0, [x23]
	add	w19, w19, 1
	cmp	w0, w19
	ble	.L14
.L10:
	ldr	x1, [sp, 144]
	mov	w0, w19
	bl	past_replay_limit
	mov	w1, w0
	mov	w0, w19
	cbnz	w1, .L14
	ldr	x1, [sp, 144]
	bl	due_ns
	mov	x27, x0
	mov	x1, x25
	bl	wait_until
	mov	x21, x0
	mov	w0, w19
	bl	check_trade
	mov	w28, w0
	mov	x1, x20
	mov	w0, 1
	bl	clock_gettime
	ldp	x3, x5, [sp, 112]
	sub	x4, x21, x27
	mov	w2, w28
	mov	w1, w19
	mov	x0, x22
	madd	x3, x3, x24, x5
	sub	x3, x3, x25
	sub	x3, x3, x21
	bl	update_stats
	cmp	x26, x21
	bgt	.L9
	ldp	x3, x2, [sp, 144]
	scvtf	d0, x21
	ldr	w1, [sp, 128]
	adrp	x0, .LC4
	add	x0, x0, :lo12:.LC4
	add	w19, w19, 1
	fdiv	d0, d0, d15
	sub	x2, x2, x3
	scvtf	d1, x2
	fdiv	d1, d1, d14
	bl	printf
	mov	x0, 58368
	movk	x0, 0x540b, lsl 16
	movk	x0, 0x2, lsl 32
	add	x26, x26, x0
	ldr	w0, [x23]
	cmp	w0, w19
	bgt	.L10
.L14:
	ldp	x27, x28, [sp, 80]
	.cfi_restore 28
	.cfi_restore 27
	ldp	d14, d15, [sp, 96]
	.cfi_restore 79
	.cfi_restore 78
.L8:
	bl	now_ns
	sub	x1, x0, x25
	mov	x0, x22
	bl	print_summary
	mov	x1, 10752
	adrp	x0, .LC5
	movk	x1, 0x1575, lsl 16
	add	x0, x0, :lo12:.LC5
	bl	printf
	ldr	w2, [x23]
	mov	w1, 40
	adrp	x0, .LC6
	add	x0, x0, :lo12:.LC6
	smull	x1, w2, w1
	bl	printf
	bl	free_trades
	ldp	x23, x24, [sp, 48]
	.cfi_restore 24
	.cfi_restore 23
	mov	w0, 0
	ldp	x25, x26, [sp, 64]
	.cfi_restore 26
	.cfi_restore 25
.L4:
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x29, x30, [sp], 192
	.cfi_remember_state
	.cfi_restore 30
	.cfi_restore 29
	.cfi_restore 21
	.cfi_restore 22
	.cfi_restore 19
	.cfi_restore 20
	.cfi_def_cfa_offset 0
	ret
.L15:
	.cfi_restore_state
	mov	x1, x19
	adrp	x0, .LC1
	add	x0, x0, :lo12:.LC1
	bl	printf
	mov	w0, 1
	b	.L4
	.cfi_endproc
.LFE29:
	.size	main, .-main
	.ident	"GCC: (Debian 14.2.0-19) 14.2.0"
	.section	.note.GNU-stack,"",@progbits
