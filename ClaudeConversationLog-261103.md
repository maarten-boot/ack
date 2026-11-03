# Conversation log 2026-11-03

- CI 41537797654 (rosco-minix68k, 4418645bd, from yesterday): all three
  jobs succeeded; schema 24, reader 120, minix204 37 passed, 0 failed,
  XFAIL core/calloc.
- User: "ok show me the parked tasks". Open in Tasks.md: F1c, P2, A3, N9,
  N6; outside it the Minix-ST calloc pin and the fork PR (rosco-minix68k 4
  commits ahead of default).
- User: "try A3 and N9". Both are upstream fixes (generic): one branch each
  off upstream default, which is still c9bccd0f0.
- A3 (misaligned structs in util/ass), scratch worktree of c9bccd0f0:
  - Sanitizer build: upstream's Makefile (PLATS = all) with CFLAGS = its own
    flags + -fsanitize=alignment, LDFLAGS and HOSTLDFLAGS
    -fsanitize=alignment (host programs link with HOSTLDFLAGS: with LDFLAGS
    only, tabgen and LLgen failed to link), UBSAN_OPTIONS=log_path. First
    attempt failed at once: -DUNREACHABLE_CODE=__builtin_unreachable()
    needs the quotes the Makefile has ("(" unexpected).
  - Before: make rc 0, 8m34s, 226 tests passed; 919 reports in 7 log files
    (the 7 em_ass runs that link the em22 examples), all at 4 (mod 8):
    ass30.c line_t 168, assrl.c relc_t 161, asscm.c freeblock 154, assci.c
    line_t 147, ass50.c line_t 49 / relc_t 42 / locl_t 8, ass40.c line_t 42
    / locl_t 8, ass80.c relc_t 42, ass30.c locl_t 34, ass70.c locl_t 32,
    ass00.c line_t 14 / stat_t 14 / locl_t 4. Addresses 0x7ff...: the
    workspace[6000] char array in main()'s frame, which ass00.c gives to
    the allocator at start.
  - Cause: getarea()/freearea() round sizes to sizeof(int) and split free
    blocks from the end; names take strlen + 1 bytes, so the next area
    lands at 4 (mod 8). linesize[] (assda.c) is BASE + ..., at least 16 on
    x86-64, so no free-block header overflow in practice.
  - Fix (asscm.c, ass00.c): sizes rounded to ALIGN = _Alignof(max_align_t)
    and to at least sizeof(struct freeblock) (roundsize()), the out-of-
    memory halving to ALIGN; workspace _Alignas(max_align_t) (6000 is a
    multiple of 16, so rounding does not run past it). As A1 for led.
  - After: incremental rebuild (2 CC, em_ass relinked, 394 ACKCC, 7
    ACKCLINK, 6 ACKLIB for em22), rc 0; 0 reports, 0 log files; em_ass is
    instrumented (__ubsan_handle_type_mismatch) and ran in the 7 links.
    Control: fix reverted and rebuilt: 919 reports again, same 7 links;
    the 7 em22 programs (share/ack/examples/*_em22.exe) of the fixed and
    the unfixed build are identical (cmp).
  - Committed on upstream/ass-align as 00365a7be (local, not pushed).
- N9 (-l on the other platforms), branch upstream/descr-lflag off c9bccd0f0:
  - All 15 plat/*/descr: `mapflag -l* LNAME={PLATFORMDIR}/lib*` (minix68k:
    `{EM}/{LIB}*`, the old Minix-ST tail_ files) -> the fork's F4 lines:
    "-lm LNAME={PLATFORMDIR}/libc.a" (the maths functions are in libc.a;
    no platform builds a libm) before "-l* LNAME={PLATFORMDIR}/lib*.a".
    util/ack takes LNAME (LIBVAR) for a -l linker input; first matching
    mapflag wins.
  - Before (staging of the A3 worktree, upstream descr files): -lm and -lc
    fail on all 14 plats with a libc.a: em_led "can't read
    .../share/ack/pc86/libm (fatal)", em22 em_ass "can't open
    .../em22/libm", minix68k ".../lib/minixST/tail_m".
  - After (plain build of the branch, all platforms): make rc 0, 8m20s,
    226 tests passed. -lm and -lc link on 13 plats (cpm, linux386,
    linux68k, linuxmips, linuxppc, minix68k, msdos386, msdos86, osx386,
    osxppc, pc86, pdpv7, rpi); -lnosuch: "can't read .../libnosuch.a";
    C program with -lbasic links (linux386, pc86, cpm). On pc86 and
    linuxppc the -lm binary is identical to the one without -l.
  - em22: -lm -> libc.a failed in em_ass ("global symbol '__ctype'
    redefined": libc.a is always linked, em_ass cannot take it twice).
    Dropped the -lm line for em22 (comment says why): now "can't open
    .../em22/libm.a". -lbasic: "Unresolved references", -lpascal:
    "'_ecvt' redeclared": em_ass limits, not the descr.
  - Run of the sqrt program: linux68k 1.414; pc86 prints 3.030 (same
    without -l: new task N13); linuxppc emulator "unimplemented
    instruction 0xffe00818 (major opcode 63)" = frsp (same without -l: new
    task N14); cpm TRAP! (no floats).
  - Committed on upstream/descr-lflag as 81521e46e (local, not pushed).
    Worktrees removed; branches upstream/ass-align and upstream/descr-lflag
    stay. Tasks.md: A3 and N9 point at the branches; N13, N14 added.
- PR text, `maarten-boot:upstream/ass-align` -> davidgiven/ack `default`:

  Title: ass: keep the areas of its allocator aligned for the host

  > em22's assembler (`util/ass`) has its own allocator in `asscm.c`.
  > `getarea()` rounds sizes to `sizeof(int)` and splits free blocks from
  > the end, so after a name (strlen + 1 bytes) the next area lands at 4
  > (mod 8). The structs kept there (`line_t`, `relc_t`, `locl_t`,
  > `stat_t`, and the allocator's own `freeblock`) hold pointers, so they
  > are misaligned on a 64-bit host. Built with `-fsanitize=alignment`,
  > `em_ass` reports 919 misaligned accesses while linking the seven em22
  > examples.
  >
  > This rounds all sizes to `_Alignof(max_align_t)` (and to at least the
  > size of a free block's header), as led does since #385, and aligns the
  > workspace in `main()` that is given to the allocator at start.
  >
  > Tested: all platforms, 226 tests pass. With `-fsanitize=alignment`:
  > 919 reports before, 0 after (and 919 again with the change reverted).
  > The seven em22 programs are byte-for-byte identical with and without
  > the change.
  >
  > 🤖 Generated with [Claude Code](https://claude.com/claude-code)

- PR text, `maarten-boot:upstream/descr-lflag` -> davidgiven/ack `default`:

  Title: plat descr: -l<name> links lib<name>.a; -lm links libc.a

  > Every platform's `descr` maps `-l<name>` to
  > `{PLATFORMDIR}/lib<name>`, without `.a`, so the linker can't open it:
  >
  >     $ ack -mpc86 -o m m.c -lm
  >     em_led: can't read .../share/ack/pc86/libm (fatal)
  >
  > `-lc` fails the same way, on all 14 platforms with a C library. minix68k
  > still maps it to the old Minix-ST `lib/minixST/tail_<name>`. This maps
  > `-l<name>` to `lib<name>.a`, and `-lm` to `libc.a`, which holds the
  > maths functions (no platform builds a `libm`), so makefiles that pass
  > `-lm` work.
  >
  > em22 gets no `-lm`: `libc.a` is always linked there, and `em_ass` can't
  > take a library twice ("global symbol '__ctype' redefined").
  >
  > Tested: all platforms, 226 tests pass. `-lm` and `-lc` link on the 13
  > platforms that use `em_led`; a missing library is now named correctly
  > (`can't read .../libnosuch.a`). On pc86 and linuxppc the program
  > linked with `-lm` is identical to the one linked without it.
  >
  > 🤖 Generated with [Claude Code](https://claude.com/claude-code)
- User: "yes commit". Committed as 06b05ed61. User pushed rosco-minix68k
  (the first push did not include the upstream branches), then
  upstream/ass-align and upstream/descr-lflag.
  - CI 41604941311 (rosco-minix68k, 06b05ed61): all three jobs succeeded;
    schema 24, reader 120, minix204 37 passed, 0 failed, XFAIL core/calloc.
  - CI 41605135921 (upstream/ass-align, 00365a7be) and 41605135492
    (upstream/descr-lflag, 81521e46e), upstream's workflow: build-linux,
    build-macos, build-windows succeeded in both.
  - Both ready for a PR to davidgiven/ack default; texts above.
- User opened the PRs: davidgiven/ack #391 (upstream/ass-align) and #392
  (upstream/descr-lflag).
  - CI 41608679766 (#391, 00365a7be) and 41608884341 (#392, 81521e46e):
    build-linux, build-macos, build-windows succeeded in both.
