#!/usr/bin/env python3
# Check a minix68k executable as written by plat/minix68k/cv, without
# running it: the 32-byte Minix-ST header, the stack size cv chose, and
# the GEMDOS relocation table (a long L, the offset of the first long to
# relocate or 0 for none, then bytes: 0 ends the table, 1 adds 254, an
# even B adds B and relocates that long). The table must end the file,
# and every relocated long must point into the image (at most the end of
# bss: the program's end pointers point exactly there).
#
#	cvcheck.py [--min-relocs N] [--min-skips N] [--stack N] <file>
#
# --stack N: the stack must be exactly N bytes. Prints one line of
# figures; on an error, says what is wrong and exits 1.

import argparse
import struct
import sys

MAGIC = 0x04100301
HEADER = 0x20
MIN_STACK = 16384  # cv.c


def check(path, min_relocs, min_skips, want_stack):
    with open(path, "rb") as f:
        b = f.read()
    if len(b) < 32:
        return "shorter than the header"
    mh = struct.unpack(">8L", b[:32])
    if mh[0] != MAGIC:
        return f"magic 0x{mh[0]:08x}, not 0x{MAGIC:08x}"
    if mh[1] != HEADER:
        return f"header length 0x{mh[1]:x}, not 0x{HEADER:x}"
    if mh[5] != 0 or mh[7] != 0:
        return f"unused header fields not 0: {mh[5]:#x} {mh[7]:#x}"
    text, data, bss, total = mh[2], mh[3], mh[4], mh[6]
    image = text + data + bss
    if text & 1 or data & 1:
        return f"odd text or data size: {text} {data}"
    if total < image:
        return f"total {total} smaller than text+data+bss {image}"
    stack = total - image
    if want_stack is not None:
        if stack != want_stack:
            return f"stack {stack}, expected {want_stack}"
    elif stack != MIN_STACK and total % 0x10000 != 0:
        # cv gives the rest of the last 64K, or MIN_STACK if that is less
        return f"stack {stack}: neither {MIN_STACK} nor up to a 64K boundary"
    if stack < MIN_STACK:
        return f"stack {stack} below {MIN_STACK}"

    size = text + data
    p = 32 + size
    if len(b) < p + 4:
        return f"file too short for text+data and the table ({len(b)} bytes)"
    (pos,) = struct.unpack(">L", b[p : p + 4])
    p += 4
    relocs = skips = 0
    if pos != 0:
        while True:
            if pos & 1 or pos + 4 > size:
                return f"relocation at 0x{pos:x}, outside text+data or odd"
            (v,) = struct.unpack(">L", b[32 + pos : 36 + pos])
            if v > image:
                return f"long at 0x{pos:x} is 0x{v:x}, beyond the end of bss 0x{image:x}"
            relocs += 1
            while True:
                if p >= len(b):
                    return "relocation table not terminated"
                c = b[p]
                p += 1
                if c != 1:
                    break
                pos += 254
                skips += 1
            if c == 0:
                break
            if c & 1:
                return f"odd step {c} in the relocation table"
            pos += c
    if p != len(b):
        return f"{len(b) - p} bytes after the relocation table"
    if relocs < min_relocs:
        return f"{relocs} relocations, expected at least {min_relocs}"
    if skips < min_skips:
        return f"{skips} skip bytes, expected at least {min_skips}"
    print(
        f"{path}: text {text} data {data} bss {bss} stack {stack}"
        f" relocs {relocs} skips {skips} table {len(b) - 32 - size}"
    )
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--min-relocs", type=int, default=0)
    ap.add_argument("--min-skips", type=int, default=0)
    ap.add_argument("--stack", type=int)
    ap.add_argument("file")
    a = ap.parse_args()
    err = check(a.file, a.min_relocs, a.min_skips, a.stack)
    if err:
        print(f"{a.file}: {err}")
        sys.exit(1)


main()
