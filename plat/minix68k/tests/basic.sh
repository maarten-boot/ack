#!/bin/sh
# Compile one of the BASIC test programs (lang/basic/test) for minix68k, run
# it in the emulator and compare its output with the expected output, as
# lang/basic/test/runcmp did.
#
#	basic.sh <ack> <emulator> <prog.bas> <expected> <tstdir>
#
# ACKDIR must point at the ACK to use. The program is compiled and run in a
# directory of its own, with a copy of lang/basic/test/tst, which some tests
# read and write. As with runcmp, the exit status of the program does
# not matter (some tests end with an error on purpose), only its output.
# stdin is /dev/null (util/build's testrunner closes it, and a BASIC
# program reading a closed stdin retries for ever); timeout is coreutils'.

abs() { case $1 in /*) echo "$1" ;; *) echo "$PWD/$1" ;; esac; }

ack=$(abs "$1")
emu=$(abs "$2")
src=$(abs "$3")
expected=$(abs "$4")
tstdir=$(abs "$5")
case $ACKDIR in /*) ;; *) ACKDIR=$PWD/$ACKDIR; export ACKDIR ;; esac

name=$(basename "$src" .bas)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cp "$src" "$work/"
cp -R "$tstdir" "$work/tst"
cd "$work" || exit 1

if ! "$ack" -mminix68k -O -o "$name" "$name.bas" > compile.out 2>&1; then
	cat compile.out
	echo "@@FAIL basic $name: compile error"
	exit 1
fi

timeout 30 "$emu" "$name" < /dev/null > out 2> err

if cmp -s out "$expected"; then
	exit 0
fi
echo "--- expected, +++ got:"
diff "$expected" out | head -40
cat err
echo "@@FAIL basic $name"
exit 1
