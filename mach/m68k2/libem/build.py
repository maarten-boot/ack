from build import PLATS
from build.ack import ackclibrary
from glob import glob
from mach.proto.fp.build import fp_sources

# fp68881.s is left out: a 68000/68010 cannot use a 68881 through F-line
# instructions (that needs a 68020 or later), so the float helpers come from
# the software package in mach/proto/fp.

for plat in PLATS:
    ackclibrary(
        name=f"lib_{plat}",
        srcs=(
            [f for f in glob("mach/m68k2/libem/*.s") if not f.endswith("/fp68881.s")]
            + glob("mach/m68k2/libem/*.c")
            + fp_sources(plat, "mach/m68k2/libfp/byte_order.h")
        ),
        plat=plat,
        deps=["h"],
    )
