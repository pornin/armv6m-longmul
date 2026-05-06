	.syntax	unified
	.cpu	cortex-m0
	.file	"aeabi_lmul.s"
	.text

@ ========================================================================
@ int64_t aeabi_lmul(int64_t x, int64_t y)
@ 64-bit multiplication (64-bit inputs, 64-bit output). Works for all
@ signed/unsigned cases.
@ Cost: 24 cycles
@ ========================================================================
	.align	1
	.global	aeabi_lmul
	.thumb
	.thumb_func
	.type	aeabi_lmul, %function
aeabi_lmul:
	@ Inputs are xL:xH (in r0:r1) and yL:yH (in r2:r3). Output
	@ is: xL*yL + (xL*yH + xH*yL)*2^32  (mod 2^64). We first compute
	@ xL*yH + xH*yL mod 2^32, with output into r12.
	muls	r1, r1, r2
	muls	r3, r3, r0
	adds	r3, r3, r1
	mov	r12, r3

	@ Now we have to compute xL*yL. In all the remaining, x0 and x1
	@ are the low and high halves of xL (16 bits each), while y0 and y1
	@ are the low and high halves of yL.
	lsrs	r1, r2, #16   @ r1 <- y1
	lsrs	r3, r0, #16   @ r3 <- x1
	muls	r1, r1, r3    @ r1 <- x1*y1
	add	r12, r1       @ r12 <- xL*yH + xH*yL + x1*y1  mod 2^32
	uxth	r1, r2        @ r1 <- y0
	muls	r3, r3, r1    @ r3 <- x1*y0
	uxth	r0, r0        @ r0 <- x0
	lsrs	r2, r2, #16   @ r2 <- y1
	muls	r2, r2, r0    @ r2 <- x0*y1
	muls	r0, r0, r1    @ r0 <- x0*y0

	@ At this point:
	@    r0    x0*y0
	@    r2    x0*y1
	@    r3    x1*y0
	@    r12   x1*y1 + xL*yH + xH*yL + x1*y1  (mod 2^32)
	@ We need to move r12 to r1, and then add both r2 and r3 with a
	@ 16-bit shift. We want to minimize the amount of 16-bit splitting
	@ and add-with-carry; moreover, the 'add' opcode involving r12
	@ cannot use an input carry (this is not supported for "high"
	@ registers). We will split x0*y1 (in r2) into a low and a high
	@ 16-bit halves; the low part can be added to r3 before the
	@ 16-bit shift (this does not overflow, because the maximum value
	@ for that sum is (2^16-1)*(2^16-1) + 2^16-1 = 2^32 - 2^16).
	uxth	r1, r2        @ r1 <- lo(x0*y1)
	adds	r3, r3, r1    @ r3 <- x1*y0 + lo(x0*y1)
	lsrs	r1, r2, #16   @ r1 <- hi(x0*y1)
	add	r1, r12       @ r1 <- hi(x0*y1) + xL*yH + xH*yL + x1*y1  [2^32]
	lsls	r2, r3, #16   @ r2 <- lo(x1*y0 + lo(x0*y1))*2^16
	lsrs	r3, r3, #16   @ r3 <- hi(x1*y0 + lo(x0*y1))
	adds	r0, r0, r2    @ add x1*y0 + lo(x0*y1) to result (part 1)
	adcs	r1, r1, r3    @ add x1*y0 + lo(x0*y1) to result (part 2)

	bx	lr
	.size	aeabi_lmul, .-aeabi_lmul
