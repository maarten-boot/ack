/*
 * Read "op a b result" lines (fpgen.c) on stdin, compute each with this
 * machine's floating point (on minix68k: mach/proto/fp in libem) and print
 * every case that differs, then a count. Exit status 1 if any differ.
 * Assumes big-endian doubles and 32-bit longs.
 */
#include <stdio.h>
#include <string.h>

union d { double d; unsigned long w[2]; };
union f { float f; unsigned long w; };

static int rdd(const char* s, union d* x)
{
	char hi[9];
	memcpy(hi, s, 8);
	hi[8] = 0;
	return sscanf(hi, "%lx", &x->w[0]) == 1 && sscanf(s + 8, "%lx", &x->w[1]) == 1;
}

int main(void)
{
	char op[8], sa[20], sb[20], sr[20];
	long n = 0, bad = 0;

	while (scanf("%7s %19s %19s %19s", op, sa, sb, sr) == 4) {
		n++;
		if (op[0] == 'f' || op[0] == 'd') {
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
				default: got.f = a.f / b.f; break;
				}
			}
			if (got.w != r.w) {
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
			} else {
				rdd(sa, &a);
				rdd(sb, &b);
				switch (op[0]) {
				case '+': got.d = a.d + b.d; break;
				case '-': got.d = a.d - b.d; break;
				case '*': got.d = a.d * b.d; break;
				default: got.d = a.d / b.d; break;
				}
			}
			if (got.w[0] != r.w[0] || got.w[1] != r.w[1]) {
				bad++;
				printf("%s %s %s want %08lx%08lx got %08lx%08lx\n", op, sa, sb,
					r.w[0], r.w[1], got.w[0], got.w[1]);
			}
		}
	}
	printf("%ld cases, %ld differ\n", n, bad);
	return bad != 0;
}
