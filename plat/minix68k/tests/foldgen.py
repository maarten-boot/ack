#!/usr/bin/env python3
"""Floating constants folded by the compiler against Python's IEEE arithmetic.

    foldgen.py <out.c> <expected.txt> <count> <seed>

Writes a C program with count random pairs of literals: each literal alone
and each pair folded with + - * /, as double and as float (normal, subnormal
and short decimal values). The program prints each constant's bits in hex,
one per line; expected.txt has "<kind> TAB <expression> TAB <hex>" in the
same order.
"""
import math
import random
import struct
import sys


def dbits(x):
    return struct.unpack(">Q", struct.pack(">d", x))[0]


def f32(x):
    return struct.unpack(">f", struct.pack(">f", x))[0]


def fbits(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def literal(rng):
    k = rng.randrange(5)
    if k == 0:  # short decimals like 0.1, 2.75
        return repr(round(rng.uniform(0, 100), rng.randrange(1, 4)))
    if k == 1:  # 17 digits, any magnitude
        return "%.17g" % (rng.uniform(1, 10) * 10.0 ** rng.randrange(-30, 30))
    if k == 2:  # a random double, printed exactly
        bits = rng.getrandbits(52) | (rng.randrange(900, 1150) << 52)
        return repr(struct.unpack(">d", struct.pack(">Q", bits))[0])
    if k == 3:  # tiny: subnormal double and float results
        return "%.17g" % (rng.uniform(1, 10) * 10.0 ** rng.randrange(-320, -290)
                          if rng.randrange(2) else
                          rng.uniform(1, 10) * 10.0 ** rng.randrange(-44, -36))
    return "%d.%d" % (rng.randrange(1000), rng.randrange(1000))


def cnum(s):
    return s if any(c in s for c in ".eE") else s + ".0"


def main():
    out, exp, count, seed = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
    rng = random.Random(seed)
    ops = {"+": lambda x, y: x + y, "-": lambda x, y: x - y,
           "*": lambda x, y: x * y, "/": lambda x, y: x / y}
    dcases, fcases = [], []
    for _ in range(count):
        a, b = cnum(literal(rng)), cnum(literal(rng))
        da, db = float(a), float(b)
        fa, fb = f32(da), f32(db)
        dcases.append(("dlit", a, "%016x" % dbits(da)))
        fcases.append(("flit", a + "f", "%08x" % fbits(fa)))
        for op, fn in ops.items():
            if op == "/" and db == 0:
                continue
            r = fn(da, db)
            if math.isfinite(r):
                dcases.append(("d" + op, "%s %s %s" % (a, op, b), "%016x" % dbits(r)))
            if op == "/" and fb == 0:
                continue
            # a float operation computed in double and rounded once is exact
            rf = fn(fa, fb)
            if abs(rf) < 3.4e38:
                fcases.append(("f" + op, "%sf %s %sf" % (a, op, b), "%08x" % fbits(f32(rf))))
    with open(out, "w") as f:
        f.write("#include <stdio.h>\n")
        f.write("static double d[] = {\n%s};\n" % "".join("\t%s,\n" % e for _, e, _ in dcases))
        f.write("static float g[] = {\n%s};\n" % "".join("\t%s,\n" % e for _, e, _ in fcases))
        f.write("""int main(void)
{
	unsigned int i;
	for (i = 0; i < sizeof d / sizeof d[0]; i++) {
		unsigned long *p = (unsigned long *)&d[i];
		printf("%08lx%08lx\\n", p[0], p[1]);
	}
	for (i = 0; i < sizeof g / sizeof g[0]; i++)
		printf("%08lx\\n", *(unsigned long *)&g[i]);
	return 0;
}
""")
    with open(exp, "w") as f:
        for case in dcases + fcases:
            f.write("\t".join(case) + "\n")


main()
