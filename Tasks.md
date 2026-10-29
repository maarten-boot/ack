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
- [ ] **F2 — heap overflow in `cv`'s relocation table.** `plat/minix68k/cv/cv.c:257`
  allocates `4 * cnt` bytes, but gaps between relocations need an extra byte per 254
  bytes (`:308`), and `*((long*)b) = curr` (`:303`) stores 8 bytes on a 64-bit host.
  Reproduced: sparse pointers in large initialised data make `cv` abort
  (`malloc(): corrupted top size`; ASan: heap-buffer-overflow at cv.c:310).
  Fix: write the table straight to the output (`put1()`/`put4()`), drop `cvlong()`
  (`:353`), check `write()`.

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
- [ ] **F5 — check the ABI against the rosco Minix 2 kernel.** `cv` writes the
  Minix-ST header (`a_cpu` 0x04, ST field order), a GEMDOS relocation table, 16-bit
  `int` message layouts, Minix 1.x syscall numbers (`plat/minix/include/minix/callnr.h`).
  *Waiting for the minix2 project.*
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

## Checked, not a problem

Warnings looked at on 2026-10-29 and found harmless: `util/arch/archiver.c:570,627,678`
(buffers are one byte larger and terminated), `util/amisc/anm.c:133` (static buffer,
terminated), `util/opt/putline.c:404` (commented in the code), `util/ego/cf/cf.c:153`
(global only used during the call), `util/led/scan.c:277` (overflow guard, meaningful
with a 16-bit `size_t` on the board), `mach/m68k2/as/mach5.c:288`, `util/led/finish.c:147`,
`util/ack/trans.c:618` (maybe-uninitialized false alarms),
`modules/src/flt_arith/flt_str2fl.c:244` (redundant check).
