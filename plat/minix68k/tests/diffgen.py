#!/usr/bin/env python3
"""Random C programs whose output must be the same with the host's C compiler
and with ACK for minix68k (T7, differential testing; see difftest.sh).

    diffgen.py <out.c> <seed> [nfuncs]

All arithmetic is done in 32-bit types (int32_t/uint32_t: int on the host,
long on minix68k) and every result is converted back to its own type, so the
size of int (16 on minix68k) does not matter; operations on one 8- or 16-bit
type are mixed in for minix68k's 16-bit int code. No undefined behaviour
except signed overflow, which wraps on both (the host compiler gets -fwrapv):
divisions are guarded, shift counts masked. Leaves are globals (run time),
parameters, locals and literals (constant folding); there are loops,
switches, arrays and a struct. The program prints every function's result
and all globals at the end. With TRACE=1 in the environment it also prints
every local after its assignment, to find the first wrong value.
"""
import os
import random
import sys

TRACE = os.environ.get("TRACE") == "1"
FIELDS = [("fa", "int8_t"), ("fb", "int32_t"), ("fc", "uint16_t"), ("fd", "uint8_t"),
          ("fe", "int16_t"), ("ff", "uint32_t")]
TYPES = ["int8_t", "uint8_t", "int16_t", "uint16_t", "int32_t", "uint32_t"]
BITS = {"int8_t": 8, "uint8_t": 8, "int16_t": 16, "uint16_t": 16, "int32_t": 32, "uint32_t": 32}


def signed(t):
    return not t.startswith("u")


def wide(t):
    return "uint32_t" if t == "uint32_t" else "int32_t"


def lit(rng, t):
    b = BITS[t]
    k = rng.randrange(4)
    if k == 0:
        v = rng.choice([0, 1, 2, 3, 7, 8, 15, 16, 31])
    elif k == 1:
        v = rng.choice([(1 << (b - 1)) - 1, 1 << (b - 1), (1 << b) - 1, 1 << (b - 2)])
    else:
        v = rng.getrandbits(b)
    if signed(t) and v >= 1 << (b - 1):
        v -= 1 << b
    if b == 32:
        if signed(t):
            if v == -(1 << 31):
                return "((int32_t)(-2147483647L - 1))"
            return "((int32_t)%dL)" % v
        return "((uint32_t)%dUL)" % v
    return "((%s)%d)" % (t, v)


class Gen:
    def __init__(self, rng):
        self.rng = rng
        self.globals = []

    def leaf(self, t, scope):
        r = self.rng.randrange(12)
        if r == 10:
            # Unused, but drawn: keeps the programs of the seeds in the T7 log.
            self.rng.randrange(8)
            return "((%s)arr_%s[%s & 7])" % (t, self.rng.choice(TYPES), self.leaf("uint8_t", scope))
        if r == 11:
            f = self.rng.choice(FIELDS)
            which = self.rng.choice(["st.", "sp->", "sa[%d]." % self.rng.randrange(3)])
            return "((%s)%s%s)" % (t, which, f[0])
        if r < 3:
            return lit(self.rng, t)
        if r < 7 or not scope:
            g = self.rng.choice(self.globals)
            return "((%s)%s)" % (t, g[0])
        v = self.rng.choice(scope)
        return "((%s)%s)" % (t, v[0])

    def expr(self, t, depth, scope):
        rng = self.rng
        if depth <= 0 or rng.randrange(5) == 0:
            return self.leaf(t, scope)
        w = wide(t)
        k = rng.randrange(12)
        if k <= 3 and BITS[t] < 32 and rng.randrange(2):
            # native: int16/uint16 arithmetic is int arithmetic on minix68k
            op = rng.choice(["+", "-", "*", "&", "|", "^", "/", "%", "<<", ">>", "<", "==", ">="])
            a = self.expr(t, depth - 1, scope)
            bb = self.expr(t, depth - 1, scope)
            if op in ("/", "%"):
                if signed(t):
                    return "((%s)((%s)%s == 0 || (%s)%s == -1 ? %s : %s %s %s))" % (
                        t, t, bb, t, bb, a, a, op, bb)
                return "((%s)((%s)%s == 0 ? %s : %s %s %s))" % (t, t, bb, a, a, op, bb)
            if op == "<<":
                return "((%s)((uint16_t)%s << (%s & %d)))" % (t, a, bb, BITS[t] - 1)
            if op == ">>":
                return "((%s)(%s >> (%s & %d)))" % (t, a, bb, BITS[t] - 1)
            return "((%s)(%s %s %s))" % (t, a, op, bb)
        if k <= 3:
            op = rng.choice(["+", "-", "*", "&", "|", "^"])
            a = self.expr(t, depth - 1, scope)
            b = self.expr(t, depth - 1, scope)
            return "((%s)((%s)%s %s (%s)%s))" % (t, w, a, op, w, b)
        if k == 4:
            op = rng.choice(["/", "%"])
            a = self.expr(t, depth - 1, scope)
            b = self.expr(t, depth - 1, scope)
            if w == "int32_t":
                guard = "((%s)%s == 0 || ((%s)%s == -1 && (%s)%s == (-2147483647L - 1)))" % (
                    w, b, w, b, w, a)
            else:
                guard = "((%s)%s == 0)" % (w, b)
            return "((%s)(%s ? (%s)%s : (%s)%s %s (%s)%s))" % (
                t, guard, w, a, w, a, op, w, b)
        if k == 5:
            a = self.expr(t, depth - 1, scope)
            b = self.expr("uint8_t", depth - 1, scope)
            if rng.randrange(2):
                return "((%s)(%s)((uint32_t)(%s)%s << ((%s) & 31)))" % (t, w, w, a, b)
            return "((%s)((%s)%s >> ((%s) & 31)))" % (t, w, a, b)
        if k == 6:
            op = rng.choice(["-", "~", "!"])
            a = self.expr(t, depth - 1, scope)
            if op == "-":
                return "((%s)(-(uint32_t)(%s)%s))" % (t, w, a)
            return "((%s)(%s(%s)%s))" % (t, op, w, a)
        if k == 7:
            op = rng.choice(["<", "<=", ">", ">=", "==", "!="])
            u = rng.choice(TYPES)
            cw = wide(u)
            a = self.expr(u, depth - 1, scope)
            b = self.expr(u, depth - 1, scope)
            return "((%s)((%s)%s %s (%s)%s))" % (t, cw, a, op, cw, b)
        if k == 8:
            u = rng.choice(TYPES)
            return "((%s)%s)" % (t, self.expr(u, depth - 1, scope))
        if k == 9:
            c = self.expr(rng.choice(TYPES), depth - 1, scope)
            a = self.expr(t, depth - 1, scope)
            b = self.expr(t, depth - 1, scope)
            return "(%s ? %s : %s)" % (c, a, b)
        if k == 10:
            op = rng.choice(["&&", "||"])
            a = self.expr(rng.choice(TYPES), depth - 1, scope)
            b = self.expr(rng.choice(TYPES), depth - 1, scope)
            return "((%s)(%s %s %s))" % (t, a, op, b)
        return self.leaf(t, scope)

    def function(self, n, out):
        rng = self.rng
        params = [("p%d" % i, rng.choice(TYPES)) for i in range(rng.randrange(4))]
        locs = [("l%d" % i, rng.choice(TYPES)) for i in range(rng.randrange(1, 5))]
        rt = rng.choice(TYPES)
        out.append("static %s f%d(%s)\n{\n" % (
            rt, n, ", ".join("%s %s" % (t, v) for v, t in params) or "void"))
        for v, t in locs:
            out.append("\t%s %s;\n" % (t, v))
        scope = list(params)
        for k, (v, t) in enumerate(locs):
            out.append("\t%s = %s;\n" % (v, self.expr(t, 3, scope)))
            if TRACE:
                out.append("\tshow(%d, (uint32_t)(%s)%s);\n" % (10000 + 100 * n + k, wide(t), v))
            scope.append((v, t))
        if rng.randrange(3) == 0:
            # a loop with a small trip count
            v, t = rng.choice(locs)
            out.append("\t{\n\t\tint16_t i;\n\t\tfor (i = 0; i < %d; i++)\n" % rng.randrange(1, 9))
            out.append("\t\t\t%s = %s;\n\t}\n" % (v, self.expr(t, 2, scope)))
        if rng.randrange(3) == 0:
            v, t = rng.choice(locs)
            out.append("\tswitch ((int16_t)(%s & 7)) {\n" % self.expr("uint8_t", 2, scope))
            for c in sorted(rng.sample(range(8), rng.randrange(1, 5))):
                out.append("\tcase %d: %s = %s; %s\n" % (
                    c, v, self.expr(t, 2, scope), rng.choice(["break;", ""])))
            out.append("\tdefault: %s = %s;\n\t}\n" % (v, self.expr(t, 2, scope)))
        if rng.randrange(4) == 0:
            out.append("\tsa[%d] = %s;\n" % (rng.randrange(3), rng.choice(["st", "*sp", "sa[0]"])))
        if rng.randrange(3) == 0:
            at = rng.choice(TYPES)
            out.append("\tarr_%s[%s & 7] = %s;\n" % (at, self.expr("uint8_t", 2, scope),
                                                    self.expr(at, 2, scope)))
        if rng.randrange(4) == 0:
            fn, ft = rng.choice(FIELDS)
            out.append("\tsp->%s = %s;\n" % (fn, self.expr(ft, 2, scope)))
        if rng.randrange(3) == 0:
            g = rng.choice(self.globals)
            out.append("\t%s = %s;\n" % (g[0], self.expr(g[1], 3, scope)))
        out.append("\treturn %s;\n}\n\n" % self.expr(rt, 3, scope))
        return params, rt

    def program(self, nfuncs):
        rng = self.rng
        out = ["#include <stdio.h>\n#include <stdint.h>\n\n"]
        for i in range(12):
            t = rng.choice(TYPES)
            self.globals.append(("g%d" % i, t))
            out.append("%s g%d = %s;\n" % (t, i, lit(rng, t)))
        for t in TYPES:
            out.append("%s arr_%s[8] = {%s};\n" % (t, t, ", ".join(lit(rng, t) for _ in range(8))))
        out.append("struct S {%s};\n" % " ".join("%s %s;" % (ft, fn) for fn, ft in FIELDS))
        init = lambda: "{%s}" % ", ".join(lit(rng, ft) for _, ft in FIELDS)
        out.append("struct S st = %s;\n" % init())
        out.append("struct S sa[3] = {%s, %s, %s};\n" % (init(), init(), init()))
        out.append("struct S *sp = &sa[1];\n")
        out.append("\nstatic void show(int16_t n, uint32_t v);\n\n")
        funcs = []
        for n in range(nfuncs):
            funcs.append((n,) + self.function(n, out))
        out.append("static void show(int16_t n, uint32_t v)\n{\n")
        out.append("\tprintf(\"%d %08lx\\n\", n, (unsigned long)v);\n}\n\n")
        out.append("int main(void)\n{\n")
        for n, params, rt in funcs:
            args = ", ".join(lit(rng, t) if rng.randrange(2) else "g%d" % rng.randrange(12)
                             for _, t in params)
            out.append("\tshow(%d, (uint32_t)(%s)f%d(%s));\n" % (n, wide(rt), n, args))
        for i, (g, t) in enumerate(self.globals):
            out.append("\tshow(%d, (uint32_t)(%s)%s);\n" % (1000 + i, wide(t), g))
        for t in TYPES:
            out.append("\t{ int16_t i; for (i = 0; i < 8; i++) show(2000 + i, (uint32_t)(%s)arr_%s[i]); }\n" % (wide(t), t))
        for k, s in enumerate(["st", "sa[0]", "sa[1]", "sa[2]"]):
            for j, (fn, ft) in enumerate(FIELDS):
                out.append("\tshow(%d, (uint32_t)(%s)%s.%s);\n" % (3000 + 10 * k + j, wide(ft), s, fn))
        out.append("\treturn 0;\n}\n")
        return "".join(out)


def main():
    out, seed = sys.argv[1], int(sys.argv[2])
    n = int(sys.argv[3]) if len(sys.argv) > 3 else 40
    with open(out, "w") as f:
        f.write(Gen(random.Random(seed)).program(n))


main()
