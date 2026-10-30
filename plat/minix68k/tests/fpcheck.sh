#!/bin/sh
# Compare the software floating point with the host's IEEE arithmetic:
# fpgen writes test cases (with fixed seeds, so every run is the same),
# fpcheck recomputes them in the minix68k emulator.
#
#	fpcheck.sh <emulator> <fpgen> <fpcheck>
#
# Fails if any case differs; prints the first differences. timeout is
# coreutils'.

emu=$1
gen=$2
check=$3

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failed=0

# run <name> <fpgen arguments...>
run() {
	name=$1
	shift
	"$gen" "$@" > "$work/cases" || { echo "@@FAIL fp $name: fpgen"; failed=1; return; }
	timeout 300 "$emu" "$check" < "$work/cases" > "$work/out" 2>&1
	status=$?
	echo "fp $name: $(tail -1 "$work/out")"
	if [ $status -ne 0 ]; then
		head -20 "$work/out"
		echo "@@FAIL fp $name"
		failed=1
	fi
}

run normal 1 20000
run subnormal 2 10000 d

exit $failed
