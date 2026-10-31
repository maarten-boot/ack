#!/bin/sh
# Run Kahan's Paranoia in the minix68k emulator and judge its report.
#
#	paranoia.sh <emulator> <paranoia program> <testrunner>
#
# Fails when the run does not end normally, or Paranoia counts any
# FAILURE, SERIOUS DEFECT or DEFECT, or more than MAX_FLAWS FLAWs, or its
# verdict is not "Excellent" (that also catches its warnings, e.g. for
# sqrt rounding or pow(0, 0)). Since F1b (Tasks.md) there is none.

MAX_FLAWS=0

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
	/appears to be Excellent/         { excellent = 1 }
	END {
		bad = !ended || failures || serious || defects || flaws > maxflaws || !excellent
		if (bad)
			printf "paranoia: ended=%d failures=%d serious=%d defects=%d flaws=%d (max %d) excellent=%d\n", \
			    ended, failures, serious, defects, flaws, maxflaws, excellent
		exit bad
	}
' && exit 0

echo "$out"
echo "@@FAIL paranoia"
exit 1
