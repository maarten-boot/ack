#!/bin/sh
# Run Kahan's Paranoia in the minix68k emulator and judge its report.
#
#	paranoia.sh <emulator> <paranoia program> <testrunner>
#
# Fails when the run does not end normally, or Paranoia counts any
# FAILURE, SERIOUS DEFECT or DEFECT, or more than MAX_FLAWS FLAWs. The
# software floating point (mach/proto/fp) has 1 known FLAW: the
# "unbalanced range", because it treats infinity as a number (F1b in
# Tasks.md). Lower MAX_FLAWS when that is fixed.

MAX_FLAWS=1

emu=$1
prog=$2
timeoutprog=$3

out=$("$timeoutprog" -t 60 -- "$emu" "$prog" < /dev/null 2>&1)

echo "$out" | awk -v maxflaws=$MAX_FLAWS '
	/^The number of  FAILUREs/        { failures = $NF + 0 }
	/^The number of  SERIOUS DEFECTs/ { serious = $NF + 0 }
	/^The number of  DEFECTs/         { defects = $NF + 0 }
	/^The number of  FLAWs/           { flaws = $NF + 0 }
	/Normal end of execution/         { ended = 1 }
	END {
		bad = !ended || failures || serious || defects || flaws > maxflaws
		if (bad)
			printf "paranoia: ended=%d failures=%d serious=%d defects=%d flaws=%d (max %d)\n", \
			    ended, failures, serious, defects, flaws, maxflaws
		exit bad
	}
' && exit 0

echo "$out"
echo "@@FAIL paranoia"
exit 1
