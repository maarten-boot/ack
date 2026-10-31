#!/bin/sh
# Floating constants folded by the compiler must be what IEEE arithmetic
# gives (F1d): foldgen.py writes a program of random literals and folded
# + - * / (fixed seeds, so every run is the same) with the expected bits,
# the program prints the bits the compiler made.
#
#	fold.sh <ack> <foldgen.py> <emulator>
#
# ACKDIR must point at the ACK to use. Fails if any constant differs;
# prints the first differences per kind.

abs() { case $1 in /*) echo "$1" ;; *) echo "$PWD/$1" ;; esac; }

ack=$(abs "$1")
gen=$(abs "$2")
emu=$(abs "$3")
case $ACKDIR in /*) ;; *) ACKDIR=$PWD/$ACKDIR; export ACKDIR ;; esac

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cd "$work" || exit 1
failed=0

for seed in 1 2 3; do
	if ! python3 -I "$gen" fold.c expected 300 $seed; then
		echo "@@FAIL fold seed $seed: foldgen.py"
		failed=1
		continue
	fi
	if ! "$ack" -mminix68k -O -o fold fold.c > compile.out 2>&1; then
		cat compile.out
		echo "@@FAIL fold seed $seed: compile error"
		failed=1
		continue
	fi
	if ! timeout 300 "$emu" fold > got; then
		echo "@@FAIL fold seed $seed: the program failed"
		failed=1
		continue
	fi
	paste expected got | awk -F'\t' -v seed=$seed '
		{ n++; if ($3 != $4) { bad++; if (shown[$1]++ < 3)
			printf "seed %s %s: %s: want %s, got %s\n", seed, $1, $2, $3, $4 } }
		END { printf "fold seed %s: %d constants, %d wrong\n", seed, n, bad; exit bad > 0 }' ||
		{ echo "@@FAIL fold seed $seed"; failed=1; }
done

exit $failed
