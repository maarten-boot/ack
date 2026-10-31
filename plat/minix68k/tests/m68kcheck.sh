#!/bin/sh
# Only 68010 user-mode instructions in the minix68k libraries and start-off
# files (T6), checked with m68kcheck without running anything. A small file
# with an FPU instruction and a privileged one must be caught, so the check
# cannot pass by finding nothing. Also: the assembler is in 68010 mode
# unless the source says .68000 (A2).
#
#	m68kcheck.sh <ack> <m68kcheck> <platdir>
#
# ACKDIR must point at the ACK to use; <platdir> is share/ack/minix68k.

abs() { case $1 in /*) echo "$1" ;; *) echo "$PWD/$1" ;; esac; }

ack=$(abs "$1")
check=$(abs "$2")
platdir=$(abs "$3")
case $ACKDIR in /*) ;; *) ACKDIR=$PWD/$ACKDIR; export ACKDIR ;; esac

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cd "$work" || exit 1
failed=0

fail() { echo "@@FAIL m68kcheck: $*"; failed=1; }

set -- "$platdir"/*.a "$platdir"/*.o
if [ ! -f "$1" ]; then
	fail "no libraries in $platdir"
	exit 1
fi
"$check" "$@" > libs.out 2>&1
status=$?
if [ $status -ne 0 ]; then
	cat libs.out
	fail "libraries: exit $status"
fi

cat > bad.s <<'EOF'
.sect .text
.sect .rom
.sect .data
.sect .bss
.sect .text
.define _bad
_bad:
	move.l d0,d1
	fmove.x fp0,fp1
	move.w sr,d0
	rts
EOF
if ! "$ack" -mminix68k -c -o bad.o bad.s > compile.out 2>&1; then
	cat compile.out
	fail "bad.s: assembler error"
else
	"$check" bad.o > bad.out 2>&1
	status=$?
	n=$(wc -l < bad.out)
	if [ $status -ne 1 ] || [ $n -ne 2 ] || ! grep -q F-line bad.out \
	    || ! grep -q privileged bad.out; then
		cat bad.out
		fail "bad.o: exit $status, $n findings (want 1, F-line and privileged)"
	fi
	"$check" -s bad.o > bad-s.out 2>&1
	status=$?
	n=$(wc -l < bad-s.out)
	if [ $status -ne 1 ] || [ $n -ne 1 ] || ! grep -q F-line bad-s.out; then
		cat bad-s.out
		fail "bad.o with -s: exit $status, $n findings (want 1, F-line only)"
	fi
fi

# The assembler takes 68010 instructions without .68010 (A2) and rejects
# them after .68000; the user-mode ones are clean for m68kcheck.
cat > m10.s <<'EOF'
.sect .text
.sect .rom
.sect .data
.sect .bss
.sect .text
	move ccr,d0
	rtd #4
EOF
if ! "$ack" -mminix68k -c -o m10.o m10.s > m10.out 2>&1; then
	cat m10.out
	fail "m10.s: 68010 instructions not accepted"
elif ! "$check" m10.o > m10-check.out 2>&1; then
	cat m10-check.out
	fail "m10.o: findings for 68010 user-mode instructions"
fi
{ echo .68000; cat m10.s; } > m00.s
if "$ack" -mminix68k -c -o m00.o m00.s > m00.out 2>&1; then
	fail "m00.s: 68010 instructions accepted after .68000"
fi

[ $failed -eq 0 ] && echo "m68kcheck: $# library files clean, bad.o caught, 68010 default"
exit $failed
