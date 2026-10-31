from build.c import cprogram

# Uses Musashi's disassembler from the linux68k emulator; the headers in
# srcs put their directories on the include path.
cprogram(
    name="m68kcheck",
    srcs=[
        "./m68kcheck.c",
        "plat/linux68k/emu/musashi/m68kdasm.c",
        "plat/linux68k/emu/musashi/m68k.h",
        "plat/linux68k/emu/m68kconf.h",
        "plat/linux68k/emu/sim.h",
    ],
    deps=["h", "modules/src/object"],
    cflags=["-DM68K_COMPILE_FOR_MAME=0"],
)
