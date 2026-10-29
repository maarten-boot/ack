from tests.plat.build import plat_testsuite

# Run in plat/minix68k/emu (a 68010 in user mode). "floats" runs on the
# software floating point in libem (mach/proto/fp); minix68k has no long long.
plat_testsuite(
    name="tests",
    plat="minix68k",
    method="plat/minix68k/emu",
    sets=["core", "bugs", "floats"],
)
