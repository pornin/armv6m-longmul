	.syntax	unified
	.cpu	cortex-m0
	.file	"longmul_us.s"
	.text

@ ========================================================================
@ int64_t longmul_us(uint32_t x, int32_t y)
@ Long multiplication (32-bit inputs, 64-bit output), mixed signedness
@ (first input is unsigned, second input is signed).
@ Cost: 20 cycles
@ ========================================================================
	.align	1
	.global	longmul_us
	.thumb
	.thumb_func
	.type	longmul_us, %function
longmul_us:
	@ Inputs are x (in r0) and y (in r1). Below, we denote x0 and x1
	@ (resp. y0 and y1) the low and high 16-bit halves of x (resp. y).
	@     0 <= x0, y0 <= 2^16 - 1
	@     0 <=    x1  <= 2^16 - 1
	@ -2^15 <=    y1  <= 2^15 - 1
	@ x = x0 + x1*2^16
	@ y = y0 + y1*2^16

	uxth	r3, r1        @ r3 <- y0
	asrs	r1, r1, #16   @ r1 <- y1
	uxth	r2, r0        @ r2 <- x0
	muls	r2, r2, r1    @ r2 <- x0*y1
	mov	r12, r2       @ save x0*y1 into r12
	lsrs	r2, r0, #16   @ r2 <- x1
	muls	r1, r1, r2    @ r1 <- x1*y1
	muls	r2, r2, r3    @ r2 <- x1*y0
	uxth	r0, r0        @ r0 <- x0
	muls	r0, r0, r3    @ r0 <- x0*y0

	@ Temporary result is in r0 (x0*y0) and r1 (x1*y1); we must add
	@ x0*y1 and x1*y0 "in the middle" (straddling over r0 and r1).
	@ We split x1*y0 into a low half (in [0, 2^16-1]) and a high half
	@ (in [-2^15, 2^15-1]); we add the high half into x0*y1, which does
	@ not overflow:
	@   -2^15*(2^16 - 1) <=   x0*y1   <= (2^15 - 1)*(2^16 - 1)
	@                  0 <= lo(x1*y0) <= 2^16 - 1
	@ hence:
	@   -2^31 < -2^31 + 2^15 <= x0*y1 + lo(x1*y0) <= 2^31 - 2^15 < 2^31 - 1

	lsrs	r3, r2, #16   @ r3 <- hi(x1*y0)
	adds	r1, r1, r3    @ add hi(x1*y0) into result
	uxth	r3, r2        @ r3 <- lo(x1*y0)
	add	r3, r12       @ r3 <- x0*y1 + lo(x1*y0)
	lsls	r2, r3, #16   @ r2 <- lo(x0*y1 + lo(x1*y0))*2^16
	asrs	r3, r3, #16   @ r3 <- hi(x0*y1 + lo(x1*y0))
	adds	r0, r0, r2    @ add x0*x1 + lo(x1*y0) into result (part 1)
	adcs	r1, r1, r3    @ add x0*x1 + lo(x1*y0) into result (part 2)
	bx	lr
	.size	longmul_us, .-longmul_us
