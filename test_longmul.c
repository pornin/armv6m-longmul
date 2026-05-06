/* ====================================================================== */
/*
 * Unit tests for longmul_uu(), longmul_ss() and aeabi_lmul().
 * Operands are pseudorandomly generated (with ChaCha8) and results are
 * compared with the platform-provided implementation of 64-bit
 * multiplication.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

uint64_t longmul_uu(uint32_t x, uint32_t y);
int64_t longmul_ss(int32_t x, int32_t y);
int64_t longmul_us(uint32_t x, int32_t y);
uint64_t aeabi_lmul(uint64_t x, uint64_t y);

/* ====================================================================== */
/*
 * ChaCha8 PRNG (for reproducible tests).
 */

/* Decode 32-bit word from bytes, little-endian. */
static inline uint32_t
dec32le(const void *src)
{
	const uint8_t *buf = src;
	return (uint32_t)buf[0]
		| ((uint32_t)buf[1] << 8)
		| ((uint32_t)buf[2] << 16)
		| ((uint32_t)buf[3] << 24);
}

/* Encode 32-bit word into bytes, little-endian. */
static inline void
enc32le(void *dst, uint32_t x)
{
	uint8_t *buf = dst;
	buf[0] = (uint8_t)x;
	buf[1] = (uint8_t)(x >> 8);
	buf[2] = (uint8_t)(x >> 16);
	buf[3] = (uint8_t)(x >> 24);
}

/* Context structure for a PRNG based on ChaCha8. */
typedef union {
	uint8_t b[40];
	uint32_t d[10];
	uint64_t q[5];
} fastrng_ctx;

/* Initialize the RNG. Only the first 32 bytes of the seed are used; if
   the seed is shorter than 32 bytes, then it is padded with zeros. */
static void
fastrng_init(fastrng_ctx *ac, const void *seed, size_t len)
{
	uint8_t tmp[32];
	if (len < sizeof tmp) {
		memcpy(tmp, seed, len);
		memset(tmp + len, 0, (sizeof tmp) - len);
	} else {
		memcpy(tmp, seed, sizeof tmp);
	}
	for (size_t u = 0; u < 8; u ++) {
		ac->d[u] = dec32le(tmp + (u << 2));
	}
	ac->q[4] = 0;
}

/* Generate more bytes. This internally produces blocks of 64 bytes, which
   are used to fill the output. Unused bytes in the last produced block
   are discarded. */
static void
fastrng(fastrng_ctx *ac, void *out, size_t len)
{
	/* First constant voluntarily differs from RFC 8439. */
	static const uint32_t CW[] = {
		0xA7C083FE, 0x3320646E, 0x79622d32, 0x6B206574
	};

	uint8_t *buf = out;
	uint64_t cc = ac->q[4];
	while (len > 0) {
		uint32_t state[16];
		memcpy(&state[0], CW, sizeof CW);
		memcpy(&state[4], ac->d, 32);
		state[12] = (uint32_t)cc;
		state[13] = (uint32_t)(cc >> 32);
		state[14] = 0;
		state[15] = 0;

		/*
		 * Only 8 rounds; ChaCha8() ought to be secure enough:
		 *    https://eprint.iacr.org/2019/1492
		 */
		for (int i = 0; i < 4; i ++) {
#define QROUND(a, b, c, d)   do { \
		state[a] += state[b]; \
		state[d] ^= state[a]; \
		state[d] = (state[d] << 16) | (state[d] >> 16); \
		state[c] += state[d]; \
		state[b] ^= state[c]; \
		state[b] = (state[b] << 12) | (state[b] >> 20); \
		state[a] += state[b]; \
		state[d] ^= state[a]; \
		state[d] = (state[d] <<  8) | (state[d] >> 24); \
		state[c] += state[d]; \
		state[b] ^= state[c]; \
		state[b] = (state[b] <<  7) | (state[b] >> 25); \
        } while (0)

			QROUND( 0,  4,  8, 12);
			QROUND( 1,  5,  9, 13);
			QROUND( 2,  6, 10, 14);
			QROUND( 3,  7, 11, 15);
			QROUND( 0,  5, 10, 15);
			QROUND( 1,  6, 11, 12);
			QROUND( 2,  7,  8, 13);
			QROUND( 3,  4,  9, 14);

#undef QROUND
		}

		for (size_t v = 0; v < 4; v ++) {
			state[v] += CW[v];
		}
		for (size_t v = 4; v < 12; v ++) {
			state[v] += ac->d[v - 4];
		}
		state[12] += (uint32_t)cc;
		state[13] += (uint32_t)(cc >> 32);
		cc ++;

		if (len >= 64) {
			for (size_t v = 0; v < 16; v ++) {
				enc32le(buf + (v << 2), state[v]);
			}
			buf += 64;
			len -= 64;
		} else {
			size_t v;
			for (v = 0; len >= 4; v ++, buf += 4, len -= 4) {
				enc32le(buf, state[v]);
			}
			uint32_t x = state[v];
			while (len -- > 0) {
				*buf ++ = (uint8_t)x;
				x >>= 8;
			}
			break;
		}
	}

	ac->q[4] = cc;
}

/* ====================================================================== */

static void
test_longmul_uu(void)
{
	printf("Test longmul_uu: ");
	fflush(stdout);

	fastrng_ctx fc;
	fastrng_init(&fc, "longmul_uu", 10);
	for (int i = 0; i < 128; i ++) {
		uint32_t tmp[64];
		fastrng(&fc, tmp, sizeof tmp);
		for (size_t j = 0; j < sizeof tmp; j += 8) {
			uint32_t x = dec32le(tmp + j);
			uint32_t y = dec32le(tmp + j + 4);
			if (i == 10) {
				if (j == 0 || j == 16) {
					x = 0xFFFFFFFF;
				}
				if (j == 8 || j == 16) {
					y = 0xFFFFFFFF;
				}
			}
			uint64_t z = longmul_uu(x, y);
			uint64_t r = (uint64_t)x * (uint64_t)y;
			if (z != r) {
				fprintf(stderr, "ERR:\n");
				fprintf(stderr, "%u * %u -> %llu (exp: %llu)\n",
					x,
					y,
					(unsigned long long)z,
					(unsigned long long)r);
				exit(EXIT_FAILURE);
			}
		}

		printf(".");
		fflush(stdout);
	}

	printf(" done.\n");
	fflush(stdout);
}

static void
test_longmul_ss(void)
{
	printf("Test longmul_ss: ");
	fflush(stdout);

	fastrng_ctx fc;
	fastrng_init(&fc, "longmul_ss", 10);
	for (int i = 0; i < 128; i ++) {
		uint32_t tmp[64];
		fastrng(&fc, tmp, sizeof tmp);
		for (size_t j = 0; j < sizeof tmp; j += 8) {
			uint32_t xu = dec32le(tmp + j);
			uint32_t yu = dec32le(tmp + j + 4);
			if (i == 10) {
				if (j == 0 || j == 16) {
					xu = 0xFFFFFFFF;
				}
				if (j == 8 || j == 16) {
					yu = 0xFFFFFFFF;
				}
			}
			int32_t x = *(int32_t *)&xu;
			int32_t y = *(int32_t *)&yu;
			int64_t z = longmul_ss(x, y);
			int64_t r = (int64_t)x * (int64_t)y;
			if (z != r) {
				fprintf(stderr, "ERR:\n");
				fprintf(stderr, "%d * %d -> %lld (exp: %lld)\n",
					x,
					y,
					(long long)z,
					(long long)r);
				exit(EXIT_FAILURE);
			}
		}

		printf(".");
		fflush(stdout);
	}
	printf(" done.\n");
	fflush(stdout);
}

static void
test_longmul_us(void)
{
	printf("Test longmul_us: ");
	fflush(stdout);

	fastrng_ctx fc;
	fastrng_init(&fc, "longmul_us", 10);
	for (int i = 0; i < 128; i ++) {
		uint32_t tmp[64];
		fastrng(&fc, tmp, sizeof tmp);
		for (size_t j = 0; j < sizeof tmp; j += 8) {
			uint32_t xu = dec32le(tmp + j);
			uint32_t yu = dec32le(tmp + j + 4);
			if (i == 10) {
				if (j == 0 || j == 16) {
					xu = 0xFFFFFFFF;
				}
				if (j == 8 || j == 16) {
					yu = 0xFFFFFFFF;
				}
			}
			uint32_t x = xu;
			int32_t y = *(int32_t *)&yu;
			int64_t z = longmul_us(x, y);
			int64_t r = (int64_t)x * (int64_t)y;
			if (z != r) {
				fprintf(stderr, "ERR:\n");
				fprintf(stderr, "%u * %d -> %lld (exp: %lld)\n",
					x,
					y,
					(long long)z,
					(long long)r);
				exit(EXIT_FAILURE);
			}
		}

		printf(".");
		fflush(stdout);
	}
	printf(" done.\n");
	fflush(stdout);
}

static void
test_aeabi_lmul(void)
{
	printf("Test aeabi_lmul: ");
	fflush(stdout);

	fastrng_ctx fc;
	fastrng_init(&fc, "aeabi_lmul", 10);
	for (int i = 0; i < 128; i ++) {
		uint32_t tmp[64];
		fastrng(&fc, tmp, sizeof tmp);
		for (size_t j = 0; j < sizeof tmp; j += 16) {
			uint32_t xL = dec32le(tmp + j +  0);
			uint32_t xH = dec32le(tmp + j +  4);
			uint32_t yL = dec32le(tmp + j +  8);
			uint32_t yH = dec32le(tmp + j + 12);
			if (i == 10) {
				if (j == 0 || j == 32) {
					xL = 0xFFFFFFFF;
					xH = 0xFFFFFFFF;
				}
				if (j == 16 || j == 32) {
					yL = 0xFFFFFFFF;
					yH = 0xFFFFFFFF;
				}
			}
			uint64_t x = (uint64_t)xL | ((uint64_t)xH << 32);
			uint64_t y = (uint64_t)yL | ((uint64_t)yH << 32);
			uint64_t z = aeabi_lmul(x, y);
			uint64_t r = x * y;
			if (z != r) {
				fprintf(stderr, "ERR:\n");
				fprintf(stderr,
					"%llu * %llu -> %llu (exp: %llu)\n",
					(unsigned long long)x,
					(unsigned long long)y,
					(unsigned long long)z,
					(unsigned long long)r);
				exit(EXIT_FAILURE);
			}
		}

		printf(".");
		fflush(stdout);
	}
	printf(" done.\n");
	fflush(stdout);
}

int
main(void)
{
	test_longmul_uu();
	test_longmul_ss();
	test_longmul_us();
	test_aeabi_lmul();
	return 0;
}
