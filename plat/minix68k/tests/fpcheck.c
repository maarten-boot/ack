/*
 * Read "op a b result" lines (fpgen.c) on stdin, compute each with this
 * machine's floating point (on minix68k: mach/proto/fp in libem) and print
 * every case that differs, then a count. Exit status 1 if any differ.
 * Any NaN matches any NaN (payload and sign are not compared). For a
 * comparison with a NaN only == and != are checked: EM's cmf has no
 * "unordered", so < or > holds. sq and rt test the C library: sqrt, and
 * printf "%.16e" read back by strtod. Assumes big-endian doubles and 32-bit
 * longs.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

union d { double d; unsigned long w[2]; };
union f { float f; unsigned long w; };

static void rdd(const char* s, union d* x)
{
	char hi[9];

	memcpy(hi, s, 8);
	hi[8] = 0;
	sscanf(hi, "%lx", &x->w[0]);
	sscanf(s + 8, "%lx", &x->w[1]);
}

static int isnand(union d* x)
{
	return (x->w[0] & 0x7ff00000L) == 0x7ff00000L
	    && ((x->w[0] & 0x000fffffL) | x->w[1]) != 0;
}

static int isnanf(union f* x)
{
	return (x->w & 0x7f800000L) == 0x7f800000L && (x->w & 0x007fffffL) != 0;
}

static int cmpbits(int lt, int eq, int gt)
{
	return lt << 3 | eq << 2 | gt << 1 | !eq;
}

int main(void)
{
	char op[8], sa[20], sb[20], sr[20];
	long n = 0, bad = 0;

	while (scanf("%7s %19s %19s %19s", op, sa, sb, sr) == 4) {
		int ok;
		n++;
		if (!strcmp(op, "c") || !strcmp(op, "fc")) {
			int want, got, mask = 0xf;
			sscanf(sr, "%x", &want);
			if (op[0] == 'c') {
				union d a, b;
				rdd(sa, &a);
				rdd(sb, &b);
				got = cmpbits(a.d < b.d, a.d == b.d, a.d > b.d);
				if (isnand(&a) || isnand(&b))
					mask = 0x5;
			} else {
				union f a, b;
				sscanf(sa, "%lx", &a.w);
				sscanf(sb, "%lx", &b.w);
				got = cmpbits(a.f < b.f, a.f == b.f, a.f > b.f);
				if (isnanf(&a) || isnanf(&b))
					mask = 0x5;
			}
			if ((got & mask) != (want & mask)) {
				bad++;
				printf("%s %s %s want %x got %x\n", op, sa, sb, want, got);
			}
		} else if ((op[0] == 'f' && strcmp(op, "f2d")) || !strcmp(op, "d2f")) {
			union f a, b, r, got;
			union d da;
			sscanf(sr, "%lx", &r.w);
			if (op[0] == 'd') {
				rdd(sa, &da);
				got.f = (float)da.d;
			} else {
				sscanf(sa, "%lx", &a.w);
				sscanf(sb, "%lx", &b.w);
				switch (op[1]) {
				case '+': got.f = a.f + b.f; break;
				case '-': got.f = a.f - b.f; break;
				case '*': got.f = a.f * b.f; break;
				case '/': got.f = a.f / b.f; break;
				default: got.f = -a.f; break;	/* fn */
				}
			}
			ok = got.w == r.w || (isnanf(&got) && isnanf(&r));
			if (!ok) {
				bad++;
				printf("%s %s %s want %08lx got %08lx\n", op, sa, sb, r.w, got.w);
			}
		} else {
			union d a, b, r, got;
			rdd(sr, &r);
			if (op[0] == 'i') {
				unsigned long l;
				sscanf(sa, "%lx", &l);
				got.d = (double)(long)l;
			} else if (!strcmp(op, "sq")) {
				rdd(sa, &a);
				got.d = sqrt(a.d);
			} else if (!strcmp(op, "rt")) {
				char buf[40];
				rdd(sa, &a);
				r = a;	/* must come back unchanged */
				sprintf(buf, "%.16e", a.d);
				got.d = strtod(buf, (char**)0);
			} else if (!strcmp(op, "f2d")) {
				union f fa;
				sscanf(sa, "%lx", &fa.w);
				got.d = (double)fa.f;
			} else {
				rdd(sa, &a);
				rdd(sb, &b);
				switch (op[0]) {
				case '+': got.d = a.d + b.d; break;
				case '-': got.d = a.d - b.d; break;
				case '*': got.d = a.d * b.d; break;
				case '/': got.d = a.d / b.d; break;
				default: got.d = -a.d; break;	/* n */
				}
			}
			ok = (got.w[0] == r.w[0] && got.w[1] == r.w[1])
			    || (isnand(&got) && isnand(&r));
			if (!ok) {
				bad++;
				printf("%s %s %s want %08lx%08lx got %08lx%08lx\n", op, sa, sb,
					r.w[0], r.w[1], got.w[0], got.w[1]);
			}
		}
	}
	printf("%ld cases, %ld differ\n", n, bad);
	return bad != 0;
}
