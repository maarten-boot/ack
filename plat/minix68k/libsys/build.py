from build.ack import ackclibrary
from glob import glob

ackclibrary(
    name="libsys",
    plat="minix68k",
    srcs=(
        glob("plat/minix68k/libsys/*.s")
        + glob("plat/minix68k/libsys/*.c")
        + glob("plat/minix68k/libsys/*.h")
    ),
    deps=["lang/cem/libcc.ansi/headers", "plat/minix68k/include"],
)
