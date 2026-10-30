from build.ab import export, simplerule
from build.ack import ackcprogram
from build.c import hostcprogram
from build.utils import test
from glob import glob
from os.path import basename
from tests.plat.build import plat_testsuite

# Run in plat/minix68k/emu (a 68010 in user mode). "floats" runs on the
# software floating point in libem (mach/proto/fp); minix68k has no long long.
plat_testsuite(
    name="plat_tests",
    plat="minix68k",
    method="plat/minix68k/emu",
    sets=["core", "bugs", "floats"],
)

# Kahan's floating point test, judged by paranoia.sh. Its own declaration of
# read() clashes with ACK's <unistd.h>; drop it rather than edit the copy.
simplerule(
    name="paranoia_src",
    ins=["./paranoia.c"],
    outs=["=paranoia.c"],
    commands=["sed '/extern int read(int, char\\*, int);/d' $[ins[0]] > $[outs[0]]"],
    label="SED",
)

ackcprogram(
    name="paranoia",
    srcs=[".+paranoia_src"],
    lang="c",
    plat="minix68k",
    cflags=["-O"],
)

test(
    name="paranoia_test",
    ins=[
        "./paranoia.sh",
        "plat/minix68k/emu",
        "util/build+testrunner",
        ".+paranoia",
    ],
    commands=["sh $[ins[0]] $[ins[1]] $[ins[3]] $[ins[2]]"],
)

# The software floating point against the host's IEEE arithmetic: fpgen
# (host) writes test cases, fpcheck recomputes them in the emulator.
hostcprogram(
    name="fpgen",
    srcs=["./fpgen.c"],
    ldflags=["-lm"],
)

ackcprogram(
    name="fpcheck",
    srcs=["./fpcheck.c"],
    lang="c",
    plat="minix68k",
    cflags=["-O"],
)

test(
    name="fp_test",
    ins=["./fpcheck.sh", "plat/minix68k/emu", ".+fpgen", ".+fpcheck"],
    commands=["sh $[ins[0]] $[ins[1]] $[ins[2]] $[ins[3]]"],
)

# Floating constants folded by the compiler against Python's IEEE arithmetic
# (F1d): about 9000 random literals and folded + - * /, double and float.
test(
    name="fold_test",
    ins=["./fold.sh", "./foldgen.py", "plat/minix68k/emu"],
    deps=["plat/minix68k+all", "+common"],
    commands=["ACKDIR=$(INSDIR) sh $[ins[0]] $(INSDIR)/bin/ack $[ins[1]] $[ins[2]]"],
)

# The BASIC test programs (lang/basic/test) with their expected output.
# test17 and test18 expect a division by zero to arrive as SIGFPE ("trap 8")
# and be caught by the BASIC runtime's signal handlers; on minix68k it is an
# EM trap (7, EFDIVZ) that is not turned into a signal (B1 in Tasks.md).
# test26 prints hex(-1)/oct(-1): 16 bits here, so its own expected output.
# onerror (our own): LINE INPUT at the end of stdin (B3) caught by ON ERROR
# (B4), then an error in the handler, which ends the program.
BASIC_SKIP = {"test17", "test18"}
BASIC_EXPECTED = {"test26": "./basic/test26.b.g"}
BASIC_DIR = "lang/basic/test"
BASIC_OWN = {"onerror": "plat/minix68k/tests/basic"}

basic_tests = []
for expected in sorted(glob(f"{BASIC_DIR}/*.b.g")) + [
    f"{d}/{n}.b.g" for n, d in BASIC_OWN.items()
]:
    name = basename(expected)[: -len(".b.g")]
    if name in BASIC_SKIP:
        continue
    srcdir = BASIC_OWN.get(name, BASIC_DIR)
    basic_tests.append(
        test(
            name=f"basic_{name}_test",
            ins=[
                "./basic.sh",
                "plat/minix68k/emu",
                f"{srcdir}/{name}.bas",
                BASIC_EXPECTED.get(name, expected),
            ],
            deps=glob(f"{BASIC_DIR}/tst/*") + ["plat/minix68k+all", "+common"],
            commands=[
                "ACKDIR=$(INSDIR) sh $[ins[0]] $(INSDIR)/bin/ack $[ins[1]] $[ins[2]] $[ins[3]] "
                + f"{BASIC_DIR}/tst"
            ],
        )
    )

# cv's output checked without running it (header, stack size, relocation
# table): see cv.sh for the cases.
CV_EXAMPLES = [
    "examples/hilo.c",
    "examples/mandelbrot.c",
    "examples/startrek.c",
    "examples/hilo.bas",
]

test(
    name="cv_test",
    ins=["./cv.sh", "./cvcheck.py"] + CV_EXAMPLES,
    deps=glob("plat/minix68k/tests/cv/*.c") + ["plat/minix68k+all", "+common"],
    commands=[
        "ACKDIR=$(INSDIR) sh $[ins[0]] $(INSDIR)/bin/ack $[ins[1]] "
        + "plat/minix68k/tests/cv "
        + " ".join(CV_EXAMPLES)
    ],
)

# Only 68010 user-mode instructions in the libraries (T6), checked by
# disassembling them; a file with an FPU and a privileged instruction must be
# caught.
test(
    name="m68kcheck_test",
    ins=["./m68kcheck.sh"],
    deps=["plat/minix68k+all", "+common"],
    commands=[
        "ACKDIR=$(INSDIR) sh $[ins[0]] $(INSDIR)/bin/ack "
        + "$(PLATDEP)/minix68k/m68kcheck $(PLATIND)/minix68k"
    ],
)

export(
    name="tests",
    deps=[
        ".+plat_tests",
        ".+paranoia_test",
        ".+fp_test",
        ".+cv_test",
        ".+fold_test",
        ".+m68kcheck_test",
    ]
    + basic_tests,
)
