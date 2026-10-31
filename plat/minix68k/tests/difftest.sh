#!/bin/sh
# Differential test (T7): random C programs from diffgen.py, compiled with
# the host's C compiler and with ACK for minix68k at every optimisation
# level, must print the same. The host compiler is the reference: the
# programs are written so that the size of int does not matter and the only
# undefined behaviour, signed overflow, wraps (-fwrapv).
#
#	difftest.sh <ack> <diffgen.py> <emulator> <hostcc> [seeds...]
#
# A seed is a number or a range (1-60); the default is the set the build
# runs. Run it by hand with a long range to look for new bugs. ACKDIR must
# point at the ACK to use. Prints one line per differing seed and level and
# the first differences; to reduce one, regenerate it with TRACE=1 (every
# local is printed) and find the first value that differs.

abs() { case $1 in /*) echo "$1" ;; *) echo "$PWD/$1" ;; esac; }

ack=$(abs "$1")
gen=$(abs "$2")
emu=$(abs "$3")
hostcc=$4
shift 4
case $ACKDIR in /*) ;; *) ACKDIR=$PWD/$ACKDIR; export ACKDIR ;; esac

# 1-60, and the seeds that only differed at -O2 and above before the T7
# fixes (ego).
[ $# -gt 0 ] || set -- 1-60 158 617 626 636 890

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cd "$work" || exit 1
failed=0
count=0

for arg in "$@"; do
	case $arg in
	*-*) seq "${arg%-*}" "${arg#*-}" ;;
	*) echo "$arg" ;;
	esac
done > seeds

while read -r seed <&3; do
	count=$((count + 1))
	if ! python3 -I "$gen" p.c "$seed"; then
		echo "@@FAIL diff seed $seed: diffgen.py"
		failed=1
		continue
	fi
	if ! $hostcc -O1 -fwrapv -fno-strict-aliasing -w -o host p.c ||
	   ! ./host > want; then
		echo "@@FAIL diff seed $seed: host compiler"
		failed=1
		continue
	fi
	for o in -O0 -O -O2 -O3 -O4; do
		# ACK's lowest level is the default, without a flag.
		flag=$o
		[ "$o" = -O0 ] && flag=
		if ! "$ack" -mminix68k $flag -o ack p.c > compile.out 2>&1; then
			grep -v warning compile.out | head -5
			echo "@@FAIL diff seed $seed $o: compile error"
			failed=1
			continue
		fi
		if ! timeout 60 "$emu" ack > got 2> run.err; then
			head -3 run.err
			echo "@@FAIL diff seed $seed $o: the program failed"
			failed=1
			continue
		fi
		if ! cmp -s want got; then
			diff want got | head -6
			echo "@@FAIL diff seed $seed $o: output differs"
			failed=1
		fi
	done
done 3< seeds

echo "diff: $count seeds, 5 levels each"
exit $failed
