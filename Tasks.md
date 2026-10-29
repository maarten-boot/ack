# Tasks — ACK minix68k (68010, rosco_m68k Minix)

Open issues found in this repository, most important first. `F<n>` refers to the
findings in `ClaudeConversationLog-261023.md` / `-261024.md`; `N<n>` are new in the
code check of 2026-10-29 (`ClaudeConversationLog-261029.md`). Nothing here has been
fixed yet. Scope: Linux host, C + BASIC for minix68k.

## High

- [x] **F1 — float/double needs a 68881 FPU** (fixed `b7748391d`). libem's float
  helpers were 68881 code (`fp68881.s`); a 68000/68010 cannot use a 68881 through
  F-line instructions, so every float operation trapped (on the board: SIGFPE; the
  rosco firmware has no FPU support or emulation, and Minix installs its own
  vectors). The software package `mach/proto/fp` is built again
  (`mach/proto/fp/build.py`) and replaces `fp68881.s` in the m68k2 libem, so link
  lines stay the same. minix68k runs the `floats` set (46/46 pass); a bit-exact
  comparison with host gcc matches except `sqrt` (1 ulp, libm).
- [ ] **F1b — soft-float is not IEEE-exact** (Kahan's Paranoia, 2026-10-29: ACK
  minix68k "Satisfactory though flawed", host gcc "Excellent"). From
  `mach/proto/fp`: `*`, `/`, `+`, `-` are "neither chopped nor correctly rounded",
  sticky bit wrong; overflow saturates at the largest number (no infinity);
  "unbalanced range" at the underflow threshold. From ACK's C library: `sqrt` not
  correctly rounded (-0.60 to +0.50 ulp), `pow(0,0)` gives 0 (should be 1), `printf`
  `%.17e` prints only 16 digits. Usable (BBC BASIC etc.), but results can differ
  from other machines in the last bit. Options: fix rounding/infinity in
  `mach/proto/fp`, or use another soft-float (e.g. Berkeley SoftFloat) built for
  16-bit `int`; fix `pow`, `sqrt`, `printf` precision in `lang/cem/libcc.ansi`.
- [x] **F2 — heap overflow in `cv`'s relocation table** (fixed 2026-10-29, see
  `ClaudeConversationLog-261029.md`). `emit_relo()` allocated `4 * cnt` bytes, but
  gaps between relocations need an extra byte per 254 bytes, and `*((long*)b) = curr`
  stored 8 bytes on a 64-bit host: sparse pointers in large initialised data made `cv`
  abort (`malloc(): corrupted top size`). Now the table is written through a small
  buffer (`relo_put1()`/`relo_put4()`), `cvlong()` is gone, `write()` is checked, and
  input the GEMDOS format cannot express (odd address, relocation at 0, duplicates)
  is a `fatal()` instead of a silently broken table.

- [x] **E1 — trap ignore mask never read** (fixed `4c255d142`) (found by the emulator, 2026-10-29). The code
  generator compiles `sim`/`lim` to the word `.ignmask`, but
  `mach/m68k2/libem/trp.s` tests `.trpim` (another variable in `boot.s`), and with the
  two bytes swapped (`btst` on memory tests a byte; bits 0-7 are at +1 on the 68000).
  Ignoring an EM trap can never work on minix68k. Fix: `btst d0,.ignmask+1` for
  0-7, `btst d0,.ignmask` for 8-15. Verified: `tests/plat/core/rck` passes.
- [x] **E2 — libem routines lose their return address when a trap returns** (fixed `4c255d142`).
  `mach/m68k2/libem/rck.s` keeps it in a1 across `jsr .trp` (`.trp` and the handler
  change a1: infinite loop, seen in `rck`); `shp.s` keeps it in a0 (same problem,
  heap exhausted); `dvi.s`/`dvu.s` simply end after `jsr .trp` (divide by zero with an
  ignored/caught trap runs into the next routine, d3/d4 not restored). Fix: push the
  return address before `jsr .trp`, restore d3/d4, then `rts`.
- [x] **E3 — `inn` on two-word sets uses the wrong bit order** (fixed `b3124962c`).
  `mach/m68020/ncg/table` (`#if WORD_SIZE==2`, `pat inn $1==4`) tests the set as one
  long (`btst bit,Dn`), but EM (and libem's `.set`/`.inn`) put bit n in word n/16 by
  address: membership tests on sets of 17-32 elements disagree with how they were
  built (Pascal/Modula-2). Fix: drop the inline rule; `inn 4` then calls `.inn`.
  Verified: `tests/plat/core/inn` passes.
- [x] **E4 — EM tests assume EM_WSIZE == EM_PSIZE or little-endian** (fixed `1159d464e`) (wrong only on
  minix68k, the one platform with 2-byte words and 4-byte pointers, big-endian):
  `aar` compares pointers with `cmu EM_WSIZE`; `cii` builds double words as
  `loc 0; loc 1` (little-endian order; `ldc` is portable); `inn` loads a 4-byte set
  with `loe`; `aar`, `inn`, `rck` (and others, harmless while tests pass) pass
  `fail()` a 4-byte code or remove it with `ass` (pops the line number as an amount).
  Fixes verified on minix68k and unchanged results on linux68k, pc86, cpm, linuxppc.

## Medium

- [ ] **F3 — default stack can be tiny.** `cv.c:60-95` (`minixhead()`) rounds
  text+data+bss up to the next 64 KB and gives the rest to the stack; just below a
  64 KB boundary that is a few bytes. Seen: `examples/paranoia.c` gets an 866-byte stack
  (text+data+bss 64,670 bytes); the minix port works around it with `acc-minix -S`. Fix: a minimum stack (e.g. 16 KB) or a `descr`
  option passing chmem-style `+N` to `cv`.
- [ ] **F4 — `plat/minix68k/descr` leftovers.** `:26-30` old Minix-ST paths
  (`SYSINCLUDES`, `C_LIB`, `OLD_C_LIB`, `MACHOPT_F`); `:57` `-l*` maps to a non-existent
  `{EM}/{LIB}*` (so `-lm` cannot work; use `{PLATFORMDIR}/lib*`); `:59` `-fp` sets an
  unused variable with an undefined `NLIB`; `:60` `-ansi` points to a missing file;
  `:25` `CPP_F` lacks `-D__minix -D__m68k -D__mc68000 -D__mc68010`; no
  `EGO_PLAT_FLAGS` and `util/ego/descr/m68k2.descr` is not installed.
- [ ] **F5 — minix68k and Minix-ST 2.0.4.**
  **Decision (2026-10-29): minix68k targets Minix-ST 2.0.4**, the system on the
  rosco_m68k board (not Minix 3: MMU, ELF, another message ABI, no 68k port).
  Findings (read-only look at `../minix-st-2.0.4`, see `ClaudeConversationLog-261029.md`):
  - The board does **not** use ACK's minix68k `libc.a`/`libsys.a`/`boot.o`: programs
    link as `crtso.o emrts.o <objects> libc.a libem.a libend.a` with Minix 2.0.4's own
    `ncrtso.s` and C library compiled by ACK; from ACK only the passes,
    `libem.a`, `libend.a` and `cv` are used.
  - 2.0.4's interface matches ACK's minix68k where they overlap: 16-bit message
    fields, the same numbers for all 50 shared calls (2.0.4 adds 13: `waitpid`,
    `setsid`, `getpgrp`, POSIX signals, symlinks, `reboot`, `svrctl`), the same
    `struct stat`. MM accepts `cv`'s header (ST layout, `a_cpu` 0x04 allowed on a
    68000) and its GEMDOS relocation table: **nothing to change in `cv`**.
  - ACK's own minix68k headers/libsys stay Minix 1.x (V7 style; POSIX calls declared
    but not implemented). They are only used by ACK's tests and examples. Optional:
    bring them to 2.0.4 later, or let the emulator also run programs linked against
    the 2.0.4 C library (as `acc-minix` links), adding the 2.0.4-only calls they use.
- [x] **P1 — take over the minix project's `-mshort` assembler fix** (done 2026-10-29).
  `mach/m68k2/as/mach1.c` (`eamode[]`) and `mach5.c` (`flag` in `ea_1()`) must be
  `long`: `(RELO2|RELPC)<<8` does not fit a 16-bit `int` when the assembler is built
  with gcc `-mshort` to run on the board (RELPC lost, `jsr d16(pc)` relocated).
  Their `src/rosco/ack/ack-minix68k.patch` has it; this fork does not.
- [ ] **P2 — let the minix project build this fork.** Its `build-ack.sh` clones
  upstream at `7afa32a0a` and applies its own patch; today's fixes (F2 `cv`, E1/E2
  libem) reach the board only if it builds `rosco-minix68k` from this repository
  instead (after P1). A change in that project, for its owner.
- [ ] **C + BASIC build target** (decided 2026-10-23, not implemented):
  `langs` parameter for `build_plat_libs()` (`plat/build.py`), `langs=("c","basic")`
  in `plat/minix68k/build.py`, a `c_basic` export in `build.py`, minix68k examples
  without `.mod`/`.p` (`examples/build.py`).
- [ ] **Linux release workflow for minix68k** (proposal in section 4 of
  `ClaudeConversationLog-261023.md`): build, smoke test, tarball for `ACKDIR=` use,
  release on `minix68k-*` tags.
- [ ] **F11/N7 — strict-aliasing UB, also in the C front end.**
  `lang/m2/comp/chk_expr.h:25-27` keep a set's refcount as `int` in an `arith` slot;
  clang `-Os` frees constant sets after their first use (proven, F11). The same class
  is in `modules/src/alloc/alloc.h:27` (`_A_st_free`, type-punned free-list head), used
  by the C front end `lang/cem` (66 of the gcc warnings) and Pascal: the C compiler
  only works because gcc does not exploit it today. Fix: add `-fno-strict-aliasing` to
  `CFLAGS` in `Makefile` (cheap, covers all of this old code); optionally also fix the
  three `chk_expr.h` macros to use `*((s) - 1)` (type `arith`).

## Low

- [ ] **N1 — `getln()` writes before its buffer.** `util/cmisc/tabgen.c:353`: after
  `while (n--)` runs out, `n` is -1 and `s[n - 1] = '\0'` writes `s[-2]` (lines longer
  than the buffer). Fix: remember the size and write `s[size - 1]`.
- [ ] **N2 — wrong printf type in stabs output.** `lang/cem/cemcom.ansi/stab.c:217`
  prints `int fd_width` with `%ld`: the bit-field width in `-g` debug info can be
  garbage on a 64-bit host. Fix: `(long)` cast. Same class, harmless output only:
  `util/amisc/anm.c:316`, `ashow.c:136-140,192,249`, `asize.c:60` (`uint32_t` with
  `%lx`/`%ld`), `util/ego/share/alloc.c:38` (`size_t` with `%u`),
  `lang/basic/src/basic.lex:332,425` (debug printfs).
- [ ] **N3 — format strings from data.** `util/arch/archiver.c:476`,
  `lang/cem/cpp.ansi/error.c:33`, `lang/basic/src/basic.g:242`, `basic.lex:247` pass a
  non-literal as the format; a `%` in a file or symbol name would be interpreted.
  Fix: `"%s"`.
- [ ] **N4 — fragile code that works today.** `lang/basic/src/func.c:20` `cv()`
  expands to two statements (used after `else` at `:96`, harmless now; wrap in
  `do { } while (0)`); `lang/basic/src/parsepar.c:39-87` indentation suggests the
  `files < 3` check is inside the loop (it is not, correctly); `util/opt/peephole.c:266`
  shifts a negative value (UB; use `~(offset)0 << amount`).
- [ ] **F6 — `plat/minix68k/boot.s`** comes from the "Bleasdale" port (`:13`, `:30`);
  `clr.l -4(a0)` (`:34`) relies on the Minix-ST stack layout; `add #010,sp` (`:55`)
  does not match what was pushed. Comment and test on rosco.
- [ ] **F7 — `modules/src/system/maketempfile.c`**: the `__minix` branch never
  triggers with this ACK (see F4 `CPP_F`); with 16-bit `unsigned` the `temper()`
  shifts (`x>>18`) are undefined. Use `unsigned long` under `__minix`.
- [ ] **F8 — `cv.c` small issues:** `:150` `%ld` for a `uint16_t`; `emits()` mallocs a
  16 KB buffer per chunk; `chmem()` (`:333`) can return an uninitialised `new`.
  `plat/minix68k/libsys/build.py:11-13` globs `plat/minix/libsys`, which does not exist.
- [ ] **F9 — build system:** `build/_sandbox.py:24,35,46` breaks with an `OBJ=`
  outside the tree (`join()` with an absolute or `..` path); `Makefile:13` refuses
  `PLATS=` on the command line (`PLATS ?=` would help upstreaming); `.drone.yml` and
  `.travis.yml` are dead.
- [ ] **N5 — signed `char` as array index** (11 places, e.g.
  `lang/cem/cemcom.ansi/LLlex.c:545-619`, `cpp.ansi/domacro.c:676`): today only
  reached with ASCII digits, but any byte ≥ 0x80 would index before the tables. Cast
  to `unsigned char` in the class macros.
- [ ] **N8 — BASIC compiles are not reproducible.** Compiling `examples/hilo.bas`
  twice with the same toolchain gives binaries that differ in one byte of code, an
  absolute address operand (0x0001C870 vs 0x0001B870): looks like uninitialised
  memory in the BASIC front end (`lang/basic/src`, `em_bem`). Matters for T3 (BASIC
  tests) and for comparing builds. Check with valgrind/ASan on `em_bem`.
- [ ] **N6 — merge commit with a placeholder identity.** `c50619962` ("sync from
  upstream", 2026-10-29) is authored by `Your Name <you@example.com>`: set
  `user.name`/`user.email` on the machine that made it. History is pushed, so leave
  the commit as it is.

## Tests to add

ACK has a test suite: `tests/plat` (about 60 small self-checking programs in sets
`core`, `bugs`, `floats`, `long-long`, `m2`, using `tests/plat/lib/test.c`, which only
needs `write()` and `_exit()`), run by `tests/plat/testdriver.sh` on a per-platform
emulator (`plat/linux68k/emu`: Musashi plus ~400 lines of syscall shim). minix68k has
no emulator and is not in `TEST_PLATS` (`build.py`), so **nothing is tested for it
today** beyond "it compiles". `lang/basic/test` has 49 BASIC programs and 47 expected
outputs (`*.b.g`), not wired into the build.

In order of value:

- [x] **T1 — a minix68k emulator, `plat/minix68k/emu`** (done, `dd125abc5`). Copy `plat/linux68k/emu` and change the loader and the syscalls:
  - Musashi as **`M68K_CPU_TYPE_68010`** with address errors on (already `OPT_ON` in
    `m68kconf.h`): 68020-only instructions, FPU instructions and odd word/long
    accesses then fail the test instead of passing silently;
  - load the `cv` output (32-byte Minix header, text, data, bss), at a load address
    other than 0, and **apply the GEMDOS relocation table**: this tests `cv`
    end to end;
  - build the Minix-ST initial stack that `boot.s` expects (argc, argv and envp
    vectors in place);
  - `trap #0` with d0 = SEND/RECEIVE/BOTH, d1 = FS/MM, a0 = message (16-bit `int`
    layout): EXIT, READ, WRITE, OPEN, CLOSE, LSEEK, BRK, GETPID, TIME, IOCTL.
- [x] **T2 — run `tests/plat` for minix68k** (done, `dd125abc5`: `core` and `bugs`,
  41 tests, all pass) (`plat/minix68k/tests/build.py`, add
  minix68k to `TEST_PLATS`): sets `core` and `bugs` first (the `.p`/`.mod` entries
  need Pascal/Modula-2, so a C-only list); `floats` **fails today** (F1: 68881 code
  on a 68010) and becomes the acceptance test for the soft-float work; `long-long` is
  not supported by the minix68k descr (no `q` size).
- [ ] **T3 — BASIC tests**: run `lang/basic/test/*.bas` under the emulator and compare
  with `*.b.g` (feed the interactive ones a fixed stdin, or skip them); covers the
  BASIC front end and runtime that bbcbasic-style work relies on.
- [ ] **T4 — `cv` regression tests (host only, no emulator):** a small script that
  links test programs and checks the output: header fields (32 bytes, sizes), the
  relocation table decodes, ends with 0, every relocated long points into the image
  (≤ end of bss). Cases: an empty program, the examples, and the F2 case (pointers
  separated by large initialised arrays: before the fix `cv` aborted). Also a program
  whose size is just under 64 KB (F3, stack size).
- [ ] **T10 — float conformance tests.** Kahan's Paranoia (the C version by Sumner and
  Gay; an unmodified copy is kept as `plat/minix68k/tests/paranoia.c`, not built yet) under the
  emulator, compared with host gcc, plus the bit-exact float/double differential
  test used for F1. To build with ACK, its own `extern int read(int, char*, int);`
  (line 121) must go: it clashes with ACK's `unistd.h`. The acceptance test for F1b.
  Note: `examples/paranoia.c` is a text adventure, not the IEEE test.
- [ ] **T5 — regression tests for the branch's earlier fixes**: initialised `long`s and
  structs with longs keep their layout (`con_mult()` `.data4` fix, check with `size`
  of data or in the emulator); `cv` header is 32 bytes (`8625a2877`).
- [ ] **T6 — 68010 instruction check without an emulator**: assemble/compile the
  runtime libraries and a set of C files to `.s` and fail on 68020/FPU mnemonics
  (`extb`, `muls.l`, `divs.l`, bit-field ops, `rtd`, scaled index, `f*`) and on
  `move sr,<ea>` (privileged on the 68010). Cheap; T1 catches the same at run time.
- [ ] **T7 — differential tests**: compile the same portable C programs (examples,
  `paranoia.c`, small integer/long/struct tests) with the host gcc and with ACK for
  minix68k, run both (natively and in the emulator), compare outputs. Mind 16-bit
  `int`: use `long` where values exceed 32767.
- [ ] **T8 — sanitizer and clang jobs for the host tools**: build with
  `-fsanitize=address,undefined` and run T2–T4 (would have caught F2); build with
  clang (still Linux) to catch strict-aliasing UB like F11/N7 before it bites.
- [ ] **T9 — CI**: run T2–T4 (and later T8) in `.github/workflows/ccpp.yml`; the
  Linux job already builds everything, so the tests add little time.

## Checked, not a problem

Warnings looked at on 2026-10-29 and found harmless: `util/arch/archiver.c:570,627,678`
(buffers are one byte larger and terminated), `util/amisc/anm.c:133` (static buffer,
terminated), `util/opt/putline.c:404` (commented in the code), `util/ego/cf/cf.c:153`
(global only used during the call), `util/led/scan.c:277` (overflow guard, meaningful
with a 16-bit `size_t` on the board), `mach/m68k2/as/mach5.c:288`, `util/led/finish.c:147`,
`util/ack/trans.c:618` (maybe-uninitialized false alarms),
`modules/src/flt_arith/flt_str2fl.c:244` (redundant check).
