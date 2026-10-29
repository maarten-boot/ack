from build.c import hostcprogram

# The Musashi engine, its configuration and sim.h are shared with the
# linux68k emulator; only the loader and the system calls differ.

hostcprogram(
    name="emu",
    srcs=[
        "plat/linux68k/emu+m68k_engine",
        "./sim.c",
        "plat/linux68k/emu/musashi/softfloat/softfloat.c",
        "plat/linux68k/emu/musashi/softfloat/softfloat-macros",
        "plat/linux68k/emu/musashi/softfloat/softfloat-specialize",
        "plat/linux68k/emu/sim.h",
        "plat/linux68k/emu/m68kconf.h",
    ],
    deps=[
        "plat/linux68k/emu+support",
    ],
    cflags=["-DM68K_COMPILE_FOR_MAME=0"],
)
