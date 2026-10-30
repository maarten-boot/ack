#!/bin/sh
# Package the ACK built by "make" for minix68k (rosco_m68k Minix-ST 2.0.4,
# 68010) as a relocatable tarball, and smoke-test the unpacked copy.
#
#	plat/minix68k/mkrelease.sh <name> [outdir]
#
# Run from the top of the tree after a successful make. Writes
#	<outdir>/ack-minix68k-<name>-linux-x86_64.tar.gz and .sha256
# The tarball holds the staging tree (.obj/staging: bin/ack, the passes,
# share/ack/minix68k, ...), the emulator as bin/minix68k-emu and a README.
# Use it with ACKDIR set to the unpacked directory, see the README.

set -e

name=$1
outdir=${2:-.}
[ -n "$name" ] || { echo "usage: $0 <name> [outdir]" >&2; exit 1; }

staging=.obj/staging
emu=.obj/plat/minix68k/emu/+emu/emu
[ -x "$staging/bin/ack" ] && [ -x "$emu" ] || {
	echo "$0: run make first" >&2
	exit 1
}

pkg=ack-minix68k-$name-linux-x86_64
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# The package.
mkdir "$work/$pkg"
cp -R "$staging/." "$work/$pkg/"
cp "$emu" "$work/$pkg/bin/minix68k-emu"
commit=$(git rev-parse --short HEAD 2>/dev/null || echo unknown)
cat > "$work/$pkg/README-minix68k.txt" <<EOF
ACK (Amsterdam Compiler Kit) for minix68k: Minix-ST on the rosco_m68k
(68010, 16-bit int, software floating point).

Built from https://github.com/maarten-boot/ack branch rosco-minix68k,
commit $commit ($name).

Unpack anywhere and point ACKDIR at the directory:

	export ACKDIR=/path/to/$pkg
	export PATH=\$ACKDIR/bin:\$PATH
	ack -mminix68k -O -o prog prog.c

bin/minix68k-emu runs a minix68k program on a 68010 emulator with a small
Minix system call layer (stdio, files, brk); any 68020/FPU instruction,
odd word/long access or NULL access stops it with an error:

	minix68k-emu prog [args...]

lib/ack/minix68k/m68kcheck checks ACK object files and archives without
running them: it reports FPU (F-line), non-68010 and privileged
instructions; -s allows the privileged ones (kernel and boot code):

	\$ACKDIR/lib/ack/minix68k/m68kcheck [-s] file.o lib.a ...

The rosco Minix port links with its own C library instead of ACK's:
crtso.o emrts.o <objects> libc.a \$ACKDIR/share/ack/minix68k/libem.a
\$ACKDIR/share/ack/minix68k/libend.a
EOF
tar -C "$work" -czf "$outdir/$pkg.tar.gz" "$pkg"
(cd "$outdir" && sha256sum "$pkg.tar.gz" > "$pkg.tar.gz.sha256")

# Smoke test: unpack elsewhere, compile with only the unpacked tree, run it.
mkdir "$work/test"
tar -C "$work/test" -xzf "$outdir/$pkg.tar.gz"
ACKDIR=$work/test/$pkg
export ACKDIR
cat > "$work/hello.c" <<'EOF'
#include <stdio.h>
#include <math.h>
static long big = 70000L;
int main(int argc, char **argv)
{
	double d = sqrt(2.0) * 1000.0;
	printf("hello %s %ld %d %.3f\n", argv[1], big, (int)sizeof(int), d);
	return 0;
}
EOF
"$ACKDIR/bin/ack" -mminix68k -O -o "$work/hello" "$work/hello.c"
got=$("$ACKDIR/bin/minix68k-emu" "$work/hello" rosco)
want="hello rosco 70000 2 1414.214"
if [ "$got" != "$want" ]; then
	echo "$0: smoke test failed: got '$got', want '$want'" >&2
	exit 1
fi
echo "$pkg.tar.gz: smoke test ok ($got)"
