# Tasks — ACK minix68k (68010, rosco_m68k Minix)

Open issues found in this repository, most important first. `F<n>` refers to the
findings in `ClaudeConversationLog-261023.md` / `-261024.md`; `N<n>` are new in the
code check of 2026-10-29 (`ClaudeConversationLog-261029.md`). Nothing here has been
fixed yet. Scope: Linux host, C + BASIC for minix68k.

## High

- [ ] **F1 — float/double needs a 68881 FPU.** `mach/m68k2/libem/build.py:8` globs
  every `.s`, including `fp68881.s`, so `.adf8`, `.mlf8`, `.cff8`, ... use FPU
  instructions and trap (F-line) on a 68010 without FPU (`printf("%g")`, BBC BASIC).
  The software float library `mach/proto/fp` lost its build recipe in `414bbe1e6`.
  Fix: port `mach/proto/fp/build.lua` (`git show 414bbe1e6^:mach/proto/fp/build.lua`)
  to `build.py` with `mach/m68k2/libfp/byte_order.h`, leave `fp68881.s` out for
  minix68k, link `libfp.a` before `libem.a` in `plat/minix68k/descr`.
  *Waiting for the decision from the minix2/bbcbasic projects.*
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
  64 KB boundary that is a few bytes. Fix: a minimum stack (e.g. 16 KB) or a `descr`
  option passing chmem-style `+N` to `cv`.
- [ ] **F4 — `plat/minix68k/descr` leftovers.** `:26-30` old Minix-ST paths
  (`SYSINCLUDES`, `C_LIB`, `OLD_C_LIB`, `MACHOPT_F`); `:57` `-l*` maps to a non-existent
  `{EM}/{LIB}*` (so `-lm` cannot work; use `{PLATFORMDIR}/lib*`); `:59` `-fp` sets an
  unused variable with an undefined `NLIB`; `:60` `-ansi` points to a missing file;
  `:25` `CPP_F` lacks `-D__minix -D__m68k -D__mc68000 -D__mc68010`; no
  `EGO_PLAT_FLAGS` and `util/ego/descr/m68k2.descr` is not installed.
- [ ] **F5 — move minix68k from the Minix 1.x ABI to Minix-ST 2.0.4.**
  **Decision (2026-10-29): minix68k targets Minix-ST 2.0.4**, the system on the
  rosco_m68k board. Not Minix 3: it needs an MMU, uses ELF and another message ABI,
  and never had a 68k port.
  - Today ACK's minix68k is Minix 1.x (Minix-ST 1.5 era: V7 style, no MMU):
    `plat/minix/include/minix/callnr.h` is the 1.x table (`NCALLS 70`,
    `AM_SYSCALL 69`, nothing after `CHROOT 61`); the libsys stubs
    (`plat/minix68k/libsys`) use the old `SIGNAL` call (48), `sgtty`/`TIOCGETP`
    ioctls, no `termios`, and the old `stat` layout (`short st_nlink`). The headers
    declare `sigaction`, `sigprocmask`, `waitpid` (`unistd.h`, `signal.h`,
    `sys/wait.h`), but nothing implements them: such programs fail to link.
    `descr` still says `NAME=minixST`, `config.h` `MACHINE ATARI`.
  - To do, once the 2.0.4 facts are in (from the minix2 project: call numbers,
    message layout with 16-bit `int`, `struct stat`, `termios` and its ioctls, signal
    frame for `sigreturn`, a.out header and relocation the kernel loads):
    `callnr.h` and the other headers; libsys stubs for `sigaction`, `sigprocmask`,
    `sigsuspend`, `sigpending`, `sigreturn`, `waitpid`, `setsid`, `getpgrp`,
    `tcgetattr`/`tcsetattr`; the `stat` layout; `cv`'s header if it differs
    (`a_cpu` 0x04, ST field order today); then the same interface in
    `plat/minix68k/emu` so the tests check what the board expects.
  - Why it matters: simple stdio/file programs probably already run on 2.0.4 (its
    call numbers are largely a superset of 1.x), but anything needing POSIX signals,
    `waitpid`, `setsid` or `termios` (e.g. BBC BASIC's raw keyboard input and line
    editing) does not work with the current libsys.
  *Waiting for the 2.0.4 facts from the minix2 project.*
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
