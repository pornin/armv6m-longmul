# Armv6-M Long Multiplication

CPUs using the Armv6-M architecture, in particular the Arm Cortex M0 and
M0+, have an efficient 32-bit multiplication opcode (`muls`) but it
returns only the low 32 bits of the product; the high 32 bits are not
computed at all by the hardware. Obtaining the high bits must be done in
software. The [Run-time ABI for the Arm®
Architecture](https://github.com/ARM-software/abi-aa/blob/daa7a94ca55973736c0e434a67a6e4bbcd35d7fa/rtabi32/rtabi32.rst)
(section 5.2) defines that a 64-bit multiplication routine should be
made available by the compiler as part of the "standard compiler helper
function library" under the name `__aeabi_lmul`:

```
long long __aeabi_lmul(long long, long long)
```

(Since the inputs have the same size as the output, the same function
works for signed and unsigned inputs.)

The `__aeabi_lmul()` implementation provided with usual C compilers
tends to be suboptimal. For instance, in GCC-13.2.1, as packaged under
the name `gcc-arm-none-eabi` on Ubuntu 24.04, this routine can be found
in */usr/lib/gcc/arm-none-eabi/13.2.1/thumb/v6-m/nofp/libgcc.a*,
sub-file *_muldi3.o*; disassembling it shows a complicated routine with
a cost of up to 61 cycles, which is quite a lot for this task. Moreover,
it is not even constant-time, meaning that it includes a conditional
jump that depends on the involved data, making that routine unsuitable
for cryptographic computations that involve secret data (the conditional
jump will leak information on the secret data through the overall
execution time).

A few better routines are floating around; e.g.
[libaeabi-cortexm0](https://github.com/bobbl/libaeabi-cortexm0/blob/a54e250c0f493678a49f4c713d3f7b128776c18b/lmul.S)
shows an implementation of `__aeabi_lmul()` that executes in only 30
cycles (and is constant-time).

In this repository, we show an optimized version of `__aeabi_lmul()`
with cost **24 cycles**. It is defined in [aeabi_lmul.s](aeabi_lmul.s),
under the name `aeabi_lmul` (without the leading `__`) mostly so that
it can be compiled along with the compiler-provided one, for test
purposes. If you want to reuse it as a *replacement* of the stock
`__aeabi_lmul()` function, rename it in the assembly source code.

Also provided at `longmul_uu()`, `longmul_ss()` and `longmul_us()`, which
implement multiplications of 32-bit integers with a 64-bit output; since
operands are shorter than the result, signedness matter, hence three
functions are provided for unsigned, signed, and mixed combinations:

```
uint64_t longmul_uu(uint32_t x, uint32_t y);
int64_t  longmul_ss( int32_t x,  int32_t y);
int64_t  longmul_us(uint32_t x,  int32_t y);
```

All three "longmul" function have cost **20 cycles**.
