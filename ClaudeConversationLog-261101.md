# Conversation log 2026-11-01

- Committed the crosscheck fixes: `7e73829fa` (libsys glob removed, Tasks
  text) and `93ec34fc4` (log). User pushed.
- CI run 41253422102 (`93ec34fc4`): build-linux, build-linux-clang and
  build-linux-sanitizers all succeeded; minix204-test 37 passed, 0 failed,
  XFAIL core/calloc as expected.
- CI run 41253830232 (`a023b06e4`): all three jobs succeeded; minix204-test
  37 passed, 0 failed, XFAIL core/calloc.
- Review feedback on davidgiven/ack PR #375 (upstream/basic-fixes, merged
  2026-10-31 as d8469af5): "could you add a test, please? Add a
  `tests/basic/data,bas.bas` file." Findings:
  - Upstream has no tests/basic; its tests live in tests/plat/<set>/ and are
    named `name,lang,flags...,.ext` (tests/plat/build.py splits on ",").
    BASIC's lang is `bas` (lib/descr/fe), so the file would be
    `tests/plat/basic/data,bas,.bas` in a new "basic" set.
  - Sets run per platform: pc86, cpm, linux68k (core, bugs, m2), linuxppc
    (+ floats). A BASIC program prints @@FINISHED / @@FAIL itself.
  - #375 is merged, so this is a follow-up PR from a new branch off
    upstream's default (e.g. upstream/basic-data-test).
  Proposed, not done (waiting for go-ahead).
- User: "yes go ahead with tests/plat/basic/data,bas,.bas".
- Branch `upstream/basic-data-test` in a scratch worktree at upstream
  `default` c06f5fef6 (contains #375's merge d8469af5). Changes (not
  committed): `tests/plat/basic/data,bas,.bas`, a "basic" set in
  `tests/plat/build.py`, enabled for linux68k and pc86. The worktree's
  Makefile has `PLATS = cpm linux68k linuxppc pc86` for testing only.
- The test READs numbers (1, -2, 3.5) and strings ("hello", "a, b"), RESTOREs
  and READs again, RESTORE 520 and READs; @@FAIL at line <n> on a mismatch.
- First build: /tmp (root volume) ran full. User chose to delete the old ack
  session's scratch (083599b2..., 4.8 GB); 13 GB free afterwards.
- Results per platform:
  - pc86, linux68k: @@FINISHED.
  - cpm: "TRAP!", also for `a = 1 : if a <> 1` without DATA: BASIC numbers
    are floating point and cpm has no floats (its tests skip "floats" too).
  - linuxppc: READ of a number stops the emulator: "fatal: rlwmi not
    supported" (insn 0x50c3a056, rlwimi, in the number conversion); an
    emulator gap, not #375. Not enabled on cpm and linuxppc.
- Negative check: with abebd5134 (DATA in the program) reverted, pc86 and
  linux68k both print "LINE 0:ERROR 28: File not found"; restored afterwards.
- Full four-platform build with the test: rc 0, 219 tests, no @@FAIL.
- Fork (rosco-minix68k, not committed): same file and set, "basic" added to
  minix68k's sets. Scratch clone build at a023b06e4 plus the change: rc 0,
  `plat/minix68k/tests/+basic/data_test` passes.
- User: "yes commit". Committed `upstream/basic-data-test` (without the
  Makefile PLATS line) and the fork's test commit. PR text:

#### upstream/basic-data-test — tests: BASIC DATA/READ/RESTORE (follow-up to #375)

- **`tests/plat/basic/data,bas,.bas`** reads numbers and strings from DATA
  (including a quoted string with a comma), RESTOREs, RESTOREs to a line, and
  checks each value; `@@FAIL at line <n>` on a mismatch. Named `name,lang,.ext`
  like the other tests in `tests/plat` (BASIC's language is `bas`), in a new
  `basic` set.
- Enabled on **linux68k** and **pc86**. With #375's DATA change reverted, both
  fail with `ERROR 28: File not found`.
- Not enabled on **cpm** (BASIC numbers are floating point; cpm has no floats,
  like the `floats` set) or **linuxppc** (the emulator stops on `rlwimi` in the
  number conversion: "rlwmi not supported").
