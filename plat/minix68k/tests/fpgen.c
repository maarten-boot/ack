/*
 * Generate test cases for the software floating point (mach/proto/fp):
 * lines "op a b result" in hex, the result computed by the host (IEEE,
 * round to nearest even). op: + - * / (double), f+ f- f* f/ (float),
 * d2f (double to float), f2d, i2d (long to double), n fn (negation),
 * c fc (comparison: the result is a hex digit with bits for <, ==, >,
 * !=), sq (sqrt, from the C library), rt (printf "%.16e" and strtod
 * must give a back: result 0); b is 0 when unused. Operands have
 * random mantissas, often with many trailing zeros or ones (exact and
 * near ties), and close or far exponents (alignment, cancellation).
 * fpcheck.c recomputes them on minix68k.
 *
 *	fpgen <seed> <count> [d|s]
 *
 * With "d", operands and results are near and below the underflow
 * threshold (subnormals). With "s", operands are often special: +-0,
 * +-infinity, NaN, the largest and smallest numbers, or large enough to
 * overflow. A finite nonzero number is never divided by zero (that
 * traps on minix68k).
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

static uint64_t s;
static uint64_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }
static int denorm, special;

static uint64_t d2u(double d) { uint64_t u; memcpy(&u, &d, 8); return u; }
static double u2d(uint64_t u) { double d; memcpy(&d, &u, 8); return d; }
static uint32_t f2u(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
static float u2f(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }

/* a double with exponent in [lo,hi] (unbiased), random sign and mantissa;
 * sometimes a mantissa with few bits set (to make exact ties) */
static double rd(int lo, int hi)
{
	uint64_t m = rnd() & 0xFFFFFFFFFFFFFull;
	int e = lo + (int)(rnd() % (unsigned)(hi - lo + 1));
	switch (rnd() % 4) {
	case 0: m &= ~((1ull << (rnd() % 52)) - 1); break; /* trailing zeros */
	case 1: m |= (1ull << (rnd() % 52)) - 1; break;    /* trailing ones */
	}
	if (e < -1022) /* subnormal: ldexp rounds it into range */
		return ldexp((rnd() & 1 ? -1.0 : 1.0) * u2d(1023ull << 52 | m), e);
	return u2d((rnd() & 1ull) << 63 | (uint64_t)(e + 1023) << 52 | m);
}

static float rf(int lo, int hi)
{
	uint32_t m = rnd() & 0x7FFFFF;
	int e = lo + (int)(rnd() % (unsigned)(hi - lo + 1));
	switch (rnd() % 4) {
	case 0: m &= ~((1u << (rnd() % 23)) - 1); break;
	case 1: m |= (1u << (rnd() % 23)) - 1; break;
	}
	if (e < -126)
		return ldexpf((rnd() & 1 ? -1.0f : 1.0f) * u2f(127u << 23 | m), e);
	return u2f((uint32_t)(rnd() & 1) << 31 | (uint32_t)(e + 127) << 23 | m);
}

static int finite_d(double d) { return isfinite(d) && (denorm || d == 0 || fabs(d) >= 0x1p-1022); }
static int finite_f(float f) { return isfinite(f) && (denorm || f == 0 || fabsf(f) >= 0x1p-126f); }

/* special mode: an operand that is often a special value */
static double sd(void)
{
	static const uint64_t pool[] = {
		0, 1ull << 63, 0x7ff0000000000000ull, 0xfff0000000000000ull,
		0x7ff8000000000000ull, 0x7fefffffffffffffull, 0xffefffffffffffffull,
		0x0010000000000000ull, 0x0000000000000001ull, 0x3ff0000000000000ull,
	};
	switch (rnd() % 3) {
	case 0: return u2d(pool[rnd() % (sizeof(pool) / sizeof(pool[0]))]);
	case 1: return rd(1000, 1023);	/* products and sums overflow */
	default: return rd(-60, 60);
	}
}

static float sf(void)
{
	static const uint32_t pool[] = {
		0, 1u << 31, 0x7f800000, 0xff800000, 0x7fc00000, 0x7f7fffff,
		0xff7fffff, 0x00800000, 0x00000001, 0x3f800000,
	};
	switch (rnd() % 3) {
	case 0: return u2f(pool[rnd() % (sizeof(pool) / sizeof(pool[0]))]);
	case 1: return rf(100, 127);
	default: return rf(-20, 20);
	}
}

static int cmpbits(int lt, int eq, int gt) { return lt << 3 | eq << 2 | gt << 1 | !eq; }

/* special mode: one case, or 0 if not usable */
static int one_special(void)
{
	int op = rnd() % 13;
	if (op == 12) {
		double a = sd();
		if (a < 0)
			return 0;
		printf("sq %016llx 0 %016llx\n", (unsigned long long)d2u(a), (unsigned long long)d2u(sqrt(a)));
		return 1;
	}
	if (op < 4) {
		double a = sd(), b = sd(), r;
		if (op == 3 && b == 0 && isfinite(a) && a != 0)
			return 0;
		r = op == 0 ? a + b : op == 1 ? a - b : op == 2 ? a * b : a / b;
		printf("%s %016llx %016llx %016llx\n", (char*[]){"+","-","*","/"}[op],
			(unsigned long long)d2u(a), (unsigned long long)d2u(b), (unsigned long long)d2u(r));
	} else if (op < 8) {
		float a = sf(), b = sf(), r;
		if (op == 7 && b == 0 && isfinite(a) && a != 0)
			return 0;
		r = op == 4 ? a + b : op == 5 ? a - b : op == 6 ? a * b : a / b;
		printf("f%s %08x %08x %08x\n", (char*[]){"+","-","*","/"}[op - 4], f2u(a), f2u(b), f2u(r));
	} else if (op == 8) {
		double a = rnd() & 1 ? sd() : rd(100, 140);	/* also overflow to float */
		printf("d2f %016llx 0 %08x\n", (unsigned long long)d2u(a), f2u((float)a));
	} else if (op == 9) {
		float a = sf();
		printf("f2d %08x 0 %016llx\n", f2u(a), (unsigned long long)d2u((double)a));
	} else if (op == 10) {
		double a = sd(), b = rnd() & 1 ? sd() : a;
		printf("n %016llx 0 %016llx\n", (unsigned long long)d2u(a), (unsigned long long)d2u(-a));
		printf("c %016llx %016llx %x\n", (unsigned long long)d2u(a), (unsigned long long)d2u(b),
			cmpbits(a < b, a == b, a > b));
	} else {
		float a = sf(), b = rnd() & 1 ? sf() : a;
		printf("fn %08x 0 %08x\n", f2u(a), f2u(-a));
		printf("fc %08x %08x %x\n", f2u(a), f2u(b), cmpbits(a < b, a == b, a > b));
	}
	return 1;
}

int main(int argc, char** argv)
{
	long n, i;
	s = strtoull(argv[1], 0, 0) * 2654435761u + 1;
	n = atol(argv[2]);
	denorm = argc > 3 && argv[3][0] == 'd';
	special = argc > 3 && argv[3][0] == 's';
	for (i = 0; i < n; ) {
		if (special) {
			i += one_special();
			continue;
		}
		int op = rnd() % 12;
		int span = (rnd() % 3 == 0) ? 60 : 3;  /* close or far exponents */
		int lo = denorm ? -1060 : -900, hi = denorm ? -1000 : 900;
		if (op < 4) {
			double a = rd(lo, hi), b, r;
			int ea; frexp(a, &ea);
			b = rd(ea - span < -1074 ? -1074 : ea - span, ea + span > 1023 ? 1023 : ea + span);
			if (op == 2 || op == 3) b = rd(-60, 60);
			if (op == 0) r = a + b;
			else if (op == 1) r = a - b;
			else if (op == 2) r = a * b;
			else r = a / b;
			if (!finite_d(r)) continue;
			printf("%s %016llx %016llx %016llx\n", (char*[]){"+","-","*","/"}[op],
				(unsigned long long)d2u(a), (unsigned long long)d2u(b), (unsigned long long)d2u(r));
		} else if (op < 8) {
			float a = rf(denorm ? -140 : -100, denorm ? -120 : 100), b, r;
			int ea; frexpf(a, &ea);
			b = rf(ea - span < -149 ? -149 : ea - span, ea + span > 127 ? 127 : ea + span);
			if (op >= 6) b = rf(-20, 20);
			if (op == 4) r = a + b;
			else if (op == 5) r = a - b;
			else if (op == 6) r = a * b;
			else r = a / b;
			if (!finite_f(r)) continue;
			printf("f%s %08x %08x %08x\n", (char*[]){"+","-","*","/"}[op - 4], f2u(a), f2u(b), f2u(r));
		} else if (op == 8) {
			double a = rd(denorm ? -150 : -126, denorm ? -126 : 127);
			float r = (float)a;
			if (!finite_f(r)) continue;
			printf("d2f %016llx 0 %08x\n", (unsigned long long)d2u(a), f2u(r));
		} else if (op == 10) {
			double a = fabs(rd(lo, hi));
			printf("sq %016llx 0 %016llx\n", (unsigned long long)d2u(a), (unsigned long long)d2u(sqrt(a)));
		} else if (op == 11) {
			printf("rt %016llx 0 0\n", (unsigned long long)d2u(rd(lo, hi)));
		} else {
			long l = (long)(int32_t)rnd();
			printf("i2d %08lx 0 %016llx\n", (unsigned long)(uint32_t)l, (unsigned long long)d2u((double)l));
		}
		i++;
	}
	return 0;
}
