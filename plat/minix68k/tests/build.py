from tests.plat.build import plat_testsuite

# Run in plat/minix68k/emu (a 68010 in user mode). "floats" needs software
# floating point (libem's float helpers use 68881 code); minix68k has no
# long long.
plat_testsuite(
    name="tests",
    plat="minix68k",
    method="plat/minix68k/emu",
    sets=["core", "bugs"],
)
