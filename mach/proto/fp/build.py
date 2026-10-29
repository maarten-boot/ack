from build.ab import simplerule
from os.path import dirname

# The software floating point package: the EM float helpers (.adf8, .cif4,
# ...) written in C. Each file is compiled to assembly for a platform and
# massage_s.lua renames the C symbols (_adf8) to the helper names (.adf8);
# the results go into that architecture's libem. fptrp.e raises EM traps.

FP = "mach/proto/fp"

SRCS = [
    "adder", "add_ext", "adf4", "adf8", "cff4", "cff8", "cfi", "cfu",
    "cif4", "cif8", "cmf4", "cmf8", "compact", "cuf4", "cuf8", "div_ext",
    "dvf4", "dvf8", "extend", "fef4", "fef8", "fif4", "fif8", "mlf4",
    "mlf8", "mul_ext", "ngf4", "ngf8", "nrm_ext", "sbf4", "sbf8",
    "sft_ext", "shifter", "sub_ext", "zrf4", "zrf8", "zrf_ext",
]

HDRS = [
    f"{FP}/FP_bias.h",
    f"{FP}/FP_shift.h",
    f"{FP}/FP_trap.h",
    f"{FP}/FP_types.h",
    f"{FP}/get_put.h",
]


def fp_sources(plat, byte_order):
    """The package as sources for an ackclibrary for plat: massaged
    assembly files and fptrp.e. byte_order is the architecture's
    byte_order.h."""
    srcs = []
    for n in SRCS:
        srcs.append(
            simplerule(
                name=f"fp_{plat}_{n}",
                ins=[f"{FP}/{n}.c", f"{FP}/massage_s.lua"],
                outs=[f"={n}.s"],
                deps=HDRS
                + [
                    byte_order,
                    f"plat/{plat}+tools",
                    f"plat/{plat}/include",
                    "+common",
                ],
                commands=[
                    "ACKDIR=$(INSDIR) $(INSDIR)/bin/ack -m"
                    + plat
                    + " $(ACKCFLAGS) -I"
                    + dirname(byte_order)
                    + " -c.s -o $[outs[0]].c.s $[ins[0]]",
                    "$(LUA) $[ins[1]] < $[outs[0]].c.s > $[outs[0]]",
                    "rm -f $[outs[0]].c.s",
                ],
                label="ACKFP",
            )
        )
    return srcs + [f"{FP}/fptrp.e"]
