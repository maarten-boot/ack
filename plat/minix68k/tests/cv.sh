#!/bin/sh
# Check what cv writes, without running the programs: compile test
# programs for minix68k and check each executable with cvcheck.py.
#
#	cv.sh <ack> <cvcheck.py> <cvdir> <examples...>
#
# ACKDIR must point at the ACK to use. Cases: the programs in <cvdir>
# (an empty program, pointers in data, the F2 relocation gaps, the F3
# stack just under 64K) and the given examples.

abs() { case $1 in /*) echo "$1" ;; *) echo "$PWD/$1" ;; esac; }

ack=$(abs "$1")
check=$(abs "$2")
dir=$(abs "$3")
shift 3
examples=
for e in "$@"; do examples="$examples $(abs "$e")"; done
case $ACKDIR in /*) ;; *) ACKDIR=$PWD/$ACKDIR; export ACKDIR ;; esac

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cd "$work" || exit 1
failed=0

fail() { echo "@@FAIL cv: $*"; failed=1; }

# build <output> <source> [flags...]
build() {
	out=$1
	shift
	if ! "$ack" -mminix68k -O -o "$out" "$@" > compile.out 2>&1; then
		cat compile.out
		fail "$out: compile error"
		return 1
	fi
}

# check <file> [cvcheck options...]
check() {
	f=$1
	shift
	python3 "$check" "$@" "$f" || fail "$f"
}

build empty "$dir/empty.c" && check empty
build ptrs "$dir/ptrs.c" && check ptrs --min-relocs 4
# two 30000-byte gaps: at least 2 * (30000 / 254) skip bytes
build relocgap "$dir/relocgap.c" && check relocgap --min-relocs 3 --min-skips 236

# text + data + bss from the header (three big-endian longs at 8)
image_size() {
	od -An -tu1 -j8 -N12 "$1" | tr -s ' \n' ' ' | awk '{
		for (i = 0; i < 3; i++)
			s += ((($(4*i+1) * 256 + $(4*i+2)) * 256 + $(4*i+3)) * 256 + $(4*i+4))
		print s }'
}

# F3: measure the image with a small pad, then pad it to 8 bytes short of
# 64K; the stack must then be MIN_STACK, not 8 bytes.
if build near64k "$dir/near64k.c" -DPAD=2; then
	pad=$((65536 - 8 - $(image_size near64k) + 2))
	if build near64k "$dir/near64k.c" -DPAD=$pad; then
		image=$(image_size near64k)
		[ "$image" -eq 65528 ] || fail "near64k: image $image, not 65528"
		check near64k --stack 16384
	fi
fi

for src in $examples; do
	name=$(basename "$src")
	build "$name.out" "$src" && check "$name.out" --min-relocs 1
done

exit $failed
