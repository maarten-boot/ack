#!/bin/sh
# Programs linked the way the rosco_m68k board links them, run in the
# minix68k emulator: the Minix 2.0.4 C library (from a minix-st-2.0.4 tree,
# built with this ACK by that tree's own src/rosco/ack/lib/Makefile), its
# crtso.o and emrts.o, and ACK's libem.a and libend.a:
#
#	crtso.o emrts.o <objects> fphook.o libc.a libm.a libem.a libend.a
#
# fphook.o (lib/fphook/fphook.c, printf's floating point) and libm.a
# (lib/math) are linked as well: libc.a only has the stub that prints
# "cannot print floating point", which ACK never replaces by itself.
#
#	minix204.sh <staging dir> <emulator> <minix-st-2.0.4 tree> <work dir>
#
# Runs tests/plat core, bugs and floats (C) and minix204/libc.c. Without a
# tree (empty argument and no ../minix-st-2.0.4) the test is skipped.

set -e

insdir=$(cd "$1" && pwd)
emu=$(cd "$(dirname "$2")" && pwd)/$(basename "$2")
minix=$3
work=$4
here=$(cd "$(dirname "$0")" && pwd)
top=$(cd "$here/../../.." && pwd)

if [ -z "$minix" ]; then
	if [ -d "$top/../minix-st-2.0.4/src/rosco/ack/lib" ]; then
		minix=$top/../minix-st-2.0.4
	else
		echo "minix204: no minix-st-2.0.4 tree (MINIX204=<dir>): @@SKIPPED"
		exit 0
	fi
fi
minix=$(cd "$minix" && pwd)
if [ ! -f "$minix/src/rosco/ack/lib/Makefile" ]; then
	echo "minix204: $minix has no src/rosco/ack/lib/Makefile" >&2
	exit 1
fi

# Expected to fail, because of the 2.0.4 library (name: reason).
xfail="core/calloc,c,: 2.0.4 calloc() does not check nelem*elsize for overflow"

rm -rf "$work"
mkdir -p "$work"
work=$(cd "$work" && pwd)

# ACK as the minix project's acc calls it.
cat > "$work/acc" <<EOF
#!/bin/sh
ACKDIR=$insdir
export ACKDIR
exec "$insdir/bin/ack" -mminix68k "\$@"
EOF
chmod +x "$work/acc"
acc=$work/acc

# The 2.0.4 library, built in a copy of the tree's layout so that the
# Makefile's relative paths (../../../lib, ../../include, ...) resolve and
# nothing is written into the tree itself.
tree=$work/tree
mkdir -p "$tree/src/rosco/ack/lib"
ln -s "$minix/include" "$tree/include"
for d in "$minix"/src/*; do
	[ "$(basename "$d")" = rosco ] || ln -s "$d" "$tree/src/"
done
for d in "$minix"/src/rosco/*; do
	[ "$(basename "$d")" = ack ] || ln -s "$d" "$tree/src/rosco/"
done
lib=$tree/src/rosco/ack/lib
cp "$minix/src/rosco/ack/lib/Makefile" "$minix/src/rosco/ack/lib/emrts.s" "$lib/"
echo "minix204: building the Minix 2.0.4 C library from $minix"
make -s -C "$lib" ACC="$acc" AAL="$insdir/bin/aal" > "$work/libc.log" 2>&1 || {
	cat "$work/libc.log"
	echo "minix204: building libc.a failed"
	exit 1
}

defs="-D__ACK__ -D__minix=1 -D__m68000=1 -D_MINIX -D_POSIX_SOURCE"
incs="-I$lib/inc -I$minix/include -I$top/tests/plat/lib -I$here/minix204"

"$acc" -O $defs $incs -I"$minix/src/lib/stdio" -c -o "$work/fphook.o" \
	"$minix/src/lib/fphook/fphook.c"
mkdir -p "$work/m"
for f in "$minix"/src/lib/math/*.c "$minix"/src/lib/math/*.s; do
	b=$(basename "$f")
	"$acc" -O $defs $incs -I"$minix/src/lib/math" -c -o "$work/m/${b%.*}.o" "$f"
done
"$insdir/bin/aal" cr "$work/libm.a" "$work"/m/*.o

"$acc" -O $defs $incs -c -o "$work/test.o" "$top/tests/plat/lib/test.c"
"$acc" -O $defs $incs -c -o "$work/emain.o" "$here/minix204/emain.c"

link() {
	"$acc" -o "$1" "$lib/crtso.o" "$lib/emrts.o" $2 "$work/fphook.o" \
		"$lib/libc.a" "$work/libm.a" \
		"$insdir/share/ack/minix68k/libem.a" "$insdir/share/ack/minix68k/libend.a"
}

pass=0
fail=0
failed=

# tests/plat: name,lang,flags,.c as in tests/plat/build.py
for src in "$top"/tests/plat/core/*.c "$top"/tests/plat/bugs/*.c \
	"$top"/tests/plat/floats/*.c; do
	name=$(basename "$(dirname "$src")")/$(basename "$src" .c)
	base=$(basename "$src" .c)
	flags=$(echo "$base" | cut -d, -f3- | tr ',' ' ')
	obj=$work/t/$(echo "$name" | tr '/,' '__')
	mkdir -p "$work/t"
	extra=
	case "$base" in *,e,*) extra=$work/emain.o ;; esac

	if "$acc" ${flags:--O} $defs $incs -c -o "$obj.o" "$src" > "$obj.log" 2>&1 &&
	   link "$obj" "$obj.o $extra $work/test.o" >> "$obj.log" 2>&1 &&
	   timeout 60 "$emu" "$obj" > "$obj.out" 2>&1 &&
	   grep -q '@@FINISHED' "$obj.out" &&
	   ! grep -q '@@FAIL\|@@SKIPPED' "$obj.out"; then
		ok=1
	else
		ok=0
	fi

	reason=$(echo "$xfail" | sed -n "s|^$name: ||p")
	if [ -n "$reason" ]; then
		if [ $ok = 1 ]; then
			echo "XPASS $name (expected to fail: $reason)"
			fail=$((fail + 1)); failed="$failed $name"
		else
			echo "XFAIL $name ($reason)"
		fi
	elif [ $ok = 1 ]; then
		pass=$((pass + 1))
	else
		echo "FAIL $name"
		cat "$obj.log" "$obj.out" 2>/dev/null | head -20
		fail=$((fail + 1)); failed="$failed $name"
	fi
done

# The library itself: output compared with libc.out.
"$acc" -O $defs $incs -c -o "$work/libc.o" "$here/minix204/libc.c"
link "$work/libc" "$work/libc.o"
if (cd "$work" && timeout 60 "$emu" ./libc > libc.got 2>&1) &&
   cmp -s "$work/libc.got" "$here/minix204/libc.out"; then
	pass=$((pass + 1))
else
	echo "FAIL minix204/libc"
	diff "$here/minix204/libc.out" "$work/libc.got" || true
	fail=$((fail + 1)); failed="$failed minix204/libc"
fi

echo "minix204: $pass passed, $fail failed$failed"
[ $fail = 0 ]
