from build.ab import export, simplerule
from build.ack import ackcprogram
from build.utils import test
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

export(name="tests", deps=[".+plat_tests", ".+paranoia_test"])
