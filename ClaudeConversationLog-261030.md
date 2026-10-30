# Claude conversation log — 2026-10-30

Continues `ClaudeConversationLog-261029.md`. Open work is in `Tasks.md`.

## Session 4

### N8 + T3: reproducible BASIC, and the BASIC tests in the emulator

User:
> i pushed (CI for `7040c0f9e` passed)
> N8 + T3

- **N8**: compiling `hilo.bas` six times gave six different EM files, differing in
  one `rom` constant: the third word of the DATA file descriptor `datfdes`.
  `lang/basic/src/gencode.c` emitted `(arith)(myitoa(strlen(datfname)))`, and
  `myitoa()` returns a `char *` to a static buffer: the "length" was a heap address.
  Now `(arith)strlen(datfname)` (10 for "hilo.bas.d"). The runtime's `_opnchn()` only
  uses the string pointer, so running programs were not affected. After the fix all
  49 programs in `lang/basic/test` compile identically three times in a row.
- **Prototype** (47 programs with `*.b.g` expected output, compiled and run in the
  emulator with stdin `/dev/null`): 33 matched. The differences, in order:
  - "Out of data" right away (checker, opg5, opg6, test19, test20): a C program doing
    the same `fscanf` reads works, so the emulator and stdio reading were fine. A new
    `-s` option in the emulator (logs every system call) showed `lseek(3, +35,
    SEEK_CUR)` with the 38-byte data file already buffered; the instruction trace and
    the symbols of a `.out` link gave `_setchan` → `fflush` → `lseek`. **L1**:
    `fflush()` on an input stream must give back the unread buffered bytes, `-count`,
    but did `+count` (since 1990, `d87444a7f`, when `fseek(stream, 0, SEEK_CUR)` was
    inlined without its minus). Fixed in `lang/cem/libcc.ansi/sys/stdio/fflush.c`
    (all platforms).
  - "File not found" (test21, test22, test28, test30): the programs use
    `lang/basic/test/tst/`; with that directory beside them they match.
  - opg3/opg4: `n ^ .5` printed 0.5, 0.25, 0.125: **L2**, the operands of `^` were
    swapped. The compiler (unchanged since 1988) pushes the base first, so the
    exponent is the first parameter of `_power()`; the 1985 runtime computed
    `exp(first * log(second))`, the 2013 modernisation (`d5f010774`) replaced it by
    `pow(first, second)`. Fixed in `lang/basic/lib/power.c` (parameters named for
    what they are, `pow(base, exponent)`), and the prototype in `lib.h`.
  - test26: `hex(-1)`/`oct(-1)` give `ffff`/`177777` with 16-bit `int` instead of
    the 32-bit `ffffffff`/`37777777777`; only those two lines differ: a minix68k
    expected output in `plat/minix68k/tests/basic/test26.b.g`.
  - test17/test18: expect a division by zero to reach the BASIC runtime as SIGFPE
    ("trap 8") and be caught by its signal handlers; on minix68k it is EM trap 7
    from the soft-float code, which libem prints and exits on: **B1**, skipped.
  - Also seen: the installed `hilo_bas_minix68k.exe` stops at start-up anywhere but
    its compile directory ("File not found" for `hilo.bas.d`): **B2**.
- **T3**: `plat/minix68k/tests/basic.sh` compiles a program for minix68k in a
  directory of its own with a copy of `tst/`, runs it in the emulator with stdin
  `/dev/null` under coreutils `timeout`, and compares the output; one test per
  program in `plat/minix68k/tests/build.py` (45: all with expected output except
  test17/test18). The first version used `util/build`'s testrunner, which closes
  stdin: a BASIC program reading input then retries "?Redo" until the stack is gone
  (**B3**, low).
- Checked: the harness fails (exit 1, a diff and `@@FAIL basic opg3`) with a wrong
  expected file (opg3.b.g and opg4.b.g are identical, so test01.b.g); a fresh clone
  with plain `make` runs 92 minix68k tests (47 + 45 BASIC), all passing.

### B2: DATA in the program

User:
> start with B2, put DATA in the program

- Before: the compiler wrote the DATA text to `<source>.bas.d` (one line per DATA
  statement) and every program opened that file at start-up, by the name used at
  compile time, as channel 0; READ read it with `fscanf`/`fgetc`, RESTORE n
  `fseek`ed to the start and skipped lines (`_seektab`: BASIC line → line index).
- Compiler (`lang/basic/src`): `datprintf()` collects the DATA text in a growing
  buffer (`basic.g`'s eight `fprintf(datfile, …)` calls); `datatable()` emits it as
  the rom string `_datatext` (always, empty without DATA) before `_seektab`;
  `gendata()` (opening the file at start-up) and the data file name/`FILE`
  (`datfname` in `bem.c`/`bem.h`/`initialize.c`, `datfile`, the unused
  `sys_filesize()` offset in `datastmt()`) are gone.
- Runtime (`lang/basic/lib`): `read.c` reads `_datatext` through `_datptr` when the
  channel is 0 (`_datint`, `_datflt` with `strtol`/`strtod`, `_datstr` for quoted
  and unquoted strings; skip white space, read, skip past the next `,` or newline;
  nothing left: error 2 "Out of data"); `_restore` moves `_datptr` to the start or
  past the DATA lines before the given line. `file.c`'s `_setchan(0)` no longer
  points the channel at a FILE (it keeps output on stdout, so the next
  `fflush(_chanwr)` is not `fflush(NULL)`).
- Checked: a fresh clone with plain `make` runs all 92 minix68k tests (45 BASIC,
  checker and test19 use RESTORE); the installed `hilo_bas_minix68k.exe` now runs in
  any directory; compiling writes no `.bas.d`; a program reading mixed DATA (ints,
  a negative, a float, a quoted string with a space, an unquoted word), RESTORE 20,
  RESTORE and reading past the end prints exactly the same as the old side-file
  build in its compile directory (byte-identical output, including "LINE 110:ERROR
  2: Out of data").

### E5: ego's cs crash

User:
> i pushed (CI for `607be73b4` passed)
> start with E5

- Reproduced with the m68k2 description switched on (`util/ego/build.py`
  `build_descr("m68k2")`, `EGO_PLAT_FLAGS` in `plat/minix68k/descr`): `em_ego -O3`
  on `tests/plat/core/exg` crashes in `cs`.
- gdb: `instrgroup(lnp=NULL)` from `gains()` (`util/ego/cs/cs_profit.c:178`);
  `avp->av_instr` 4 (`adp`, an "addressing mode" in the description's `%%CS` list),
  `avp->av_found` the first line of its basic block, so `l_prev` is NULL. `gains()`
  asks whether the operand comes from a simple load; the `ads` case just above
  checks `l_prev`, this one did not.
- Fix: return "no gain" when there is no previous instruction (the operand comes
  from before the block; nothing is eliminated). Only changes behaviour where `cs`
  used to crash. minix68k now uses `m68k2.descr`.
- Checked:
  - the whole suite with all minix68k libraries and tests built at `-O3`: 92/92
    (before the fix `cs` crashed);
  - the normal build (`-O`): 92/92;
  - against the normal libraries: `mandelbrot.c` and Kahan's Paranoia at `-O`,
    `-O3`, `-O4`, `-O6` give identical output (Paranoia: 2 FLAWs at every level);
    all 43 C and EM test programs of `tests/plat` (core, bugs, floats; the `,e,`
    ones linked with `-.e` as the suite does) pass at `-O4` and `-O6`.
- Found on the way, not caused by this: building ACK's own libraries at `-O4`
  (`make ACKCFLAGS=-O4`) fails to link the examples ("Undefined: __II0, __II5",
  ego inliner names), with or without the description: E6.

### T8: a sanitizer CI job

User:
> i pushed (CI for `e03f1a3f7` passed)
> start with T8

- Built every host tool with `CC=HOSTCC="gcc -fsanitize=address,undefined …"` (in
  `CC` so links that ignore `LDFLAGS` get it too) and ran `make` with all tests.
  Leak checking off (`ASAN_OPTIONS=detect_leaks=0`): the passes never free.
- Findings, first in recover mode to collect them, then strict
  (`-fno-sanitize-recover=all`) with `ninja -k 0` so every test runs (the test
  driver hides the output of passing tests, so recover mode missed UB inside the
  emulator; `make -k` does not reach ninja):
  - **S1** ASan: `util/opt/putline.c:138` read 25 bytes after `em_flag` (133
    entries): `instr` 159 = `ps_mes` with no operand; pseudos went through the
    instruction table for the control-flow and the `sp_cend` decisions. Fixed: only
    instructions use `em_flag`; a pseudo without its operand always gets `sp_cend`
    (em_code's `CC_endnarg` writes `ps_end` + `sp_cend` the same way).
  - **S2** UBSan undefined shifts: LLgen's bit-set macros (`1 << 31`), cpp's and
    cemcom's `arith_sign` (`1L << 63`; cpp has its own `arith.h`), `peephole.c`
    (twice), cemcom/m2/pc constant folding (`(x << n) >> n` sign extension of
    negatives, `bt = (bt << 8) + 0377` overflowing), ncg `rombit`, ego `cset.c`
    (three), Musashi `divs` (`remainder << 16`, four). All shifted as unsigned.
  - **A1** UBSan alignment in `util/led` and `modules/src/object/rd.c`: left out of
    the job for now (task).
- Result: strict mode, the full build and all 92 minix68k tests: 0 ASan, 0 UBSan
  reports, 1 min 55 s on 4 cores. Normal build before and after: all 107 minix68k
  programs/objects and 521 archive members byte-identical.
- `.github/workflows/ccpp.yml`: new job `build-linux-sanitizers` (same pinned
  checkout, re-checked: v7.0.1 is still the latest release and the hash a commit);
  the YAML parses.

### Small leftovers, 1: a clang CI job

User:
> i pushed (CI for `71763e7d4` passed: build-linux 82 s, build-linux-sanitizers 136 s)
> can we do 5 first or do you expect more items from the other to be added ?
> start with the clang job

- Answered: the small items can go first; F1b/A1 and the clang job may add findings;
  upstream contributions are best last (then they include everything).
- A clang 21 build (`make CC=clang HOSTCC=clang`): builds, all 92 minix68k tests
  pass (the Modula-2 failure of 2026-10-24 is gone with `-fno-strict-aliasing`).
- Its 65 warnings: mostly EM opcodes > 127 in a signed `char`/`byte` (by design, masked
  with `BMASK`), the known `led/scan.c` overflow guard, an intended assignment in a
  condition (`cemcom.ansi/arith.c:57`), dangling elses that match the indentation
  (`cemcom.ansi/eval.c`), and **M1**, two real Modula-2 bugs: `! df->df_kind &
  (D_ERROR|D_TYPE)` (`type.c:674`, always 0 with D_TYPE 0x10 and D_ERROR 0x4000) and
  `!end_reached & REACH_FLAG` (`walk.c:635`); now `!(… & …)`. After the fix a test
  program gets "statement not reached" after a RETURN in a loop; the type.c path (opaque
  types) was not reached by my test.
- `.github/workflows/ccpp.yml`: third job `build-linux-clang` (apt `clang`, `make
  CC=clang HOSTCC=clang`). Fresh clones with all changes: gcc 92/92, clang 92/92.

### Small leftovers, 2: E6, ego's inliner at -O4

User:
> i pushed (CI for `9bab97f62` passed: build-linux 78 s, sanitizers 96 s, clang 64 s)
> start with E6

- Reproduced (`make ACKCFLAGS=-O4`): `read.o` in `libbasic.a` references `__II2` and
  `__II3`, the names of its own static functions, as undefined; at `-O3` the object
  has 12 procedures, at `-O4` 10: `_datint`/`_datflt` (each called once) were
  removed, the `jsr __II2`/`jsr __II3` in `_readint`/`_readflt` stayed.
- A 20-line C reproducer (two static functions, each called once inside an `if`)
  does the same; `em_ego -Q` reports "inline substitutions: 0". An inliner built with
  `VERBOSE` says "singles: procedure 1 can be removed" for both; in gdb the callers'
  `P_CALS` lists are empty in pass 3 although `select_calls()` had marked the calls
  selected.
- gdb prints the call's `cl_flags` as `false`/`true`: `cl_flags` is `bool`. It holds
  bit flags (CLF_INLPARS 017, CLF_SELECTED 020, CLF_EVER_EXPANDED 040, CLF_FIRM
  0100), declared `bool` in 1984 when ego had `typedef byte bool`; the 2025 stdbool
  commit (`0782bc80e`) replaced that typedef by `_Bool`. `SELECTED()` stored 1,
  `IS_SELECTED()` (`& 020`) gave 0, `cleancals()` dropped the call, and the callee,
  already marked dispensable by `singles()`, was removed. The other `bool` fields
  in ego are real yes/no values. Fixed: `byte cl_flags`.
- Checked: the reproducer has no undefined names and reports 2 substitutions; clean
  builds of everything (libraries, examples, tests) at `-O4` and `-O6`: all 92
  minix68k tests pass; the normal build is byte-identical before and after (107
  programs/objects, 521 archive members).

### Small leftovers, 3: F7, `sys_maketempfile()` with 16-bit `int`

User:
> you can commit (E6: `76abdb282`, `32d145a67`)
> you can continue

- The `__minix` branch of `modules/src/system/maketempfile.c` is now reached (F4 added
  `-D__minix` to `CPP_F`). Its private `rand_r()`/`temper()` used `unsigned`, 16 bits
  on Minix-ST: `x>>18` is undefined, the `0x9D2C5680` masks are lost, and the hash
  passed to `%x` is at most 15 bits.
- Fixed: under `WIN32 || __minix` the seed and `temper()` are `unsigned long`
  (`seed_t`); the helper is renamed `next_random()` (a Minix libc that declares
  `rand_r()` would clash, as glibc does with `-D__minix` on the host); on other
  systems `next_random()` wraps the real `rand_r()`. The hash is `unsigned long`,
  printed with `%lx`.
- Checked: host gcc `-Wall -Wextra` with and without `-D__minix`; full build of a
  fresh clone, all tests pass; a test program built with `ack -mminix68k` and run in
  the emulator (`int=2 long=4`) gives `ack.tst-797c7f8d.dat`, `-61755400`, `-76a9c7ba`
  (old code: `-3933`, `-735`, `-338e`); with the file kept, `O_EXCL` moves on to a
  new name.

### Small leftovers, 4: T4, `cv` checked without the emulator

User:
> yes commit (F7: `d278a1f22`, `0e6405a91`)
> start with T4

- `plat/minix68k/tests/cvcheck.py` checks one executable: magic `0x04100301`, header
  length 0x20, unused fields 0, even text/data sizes, stack (`--stack N` exact,
  otherwise 16384 or up to a 64K boundary as `cv` chooses, never below 16384), the
  GEMDOS relocation table (start offset even and inside text+data, steps even, 1 =
  skip 254, ends with 0 exactly at the end of the file), and every relocated long ≤
  text+data+bss. `--min-relocs`/`--min-skips` for the cases that need them.
- `plat/minix68k/tests/cv.sh` (test `cv_test` in `tests/build.py`, part of `make`)
  builds `tests/cv/empty.c`, `ptrs.c` (pointers to text, rom, data, bss),
  `relocgap.c` (F2: two 30000-byte initialised arrays between pointers, ≥ 236 skip
  bytes), `near64k.c` (F3: built once to measure, then a bss pad makes
  text+data+bss = 65528; the stack must be 16384 and the image exactly 65528), and
  the examples hilo.c, mandelbrot.c, startrek.c, hilo.bas.
- Results (fresh clone, `make`): all pass; e.g. relocgap relocs 46, skips 236, table
  286 bytes; startrek 2664 relocations, stack 16384.
- The checker itself: seven damaged copies of the relocgap output (magic, a byte cut
  off, a byte added, an odd step, a relocated long beyond bss, total 100 bytes short,
  start offset past text+data) are each rejected with the right message. With the
  pre-F2 `cv` (`8625a2877`) put into the staging tree, `cv.sh` fails on relocgap
  (`malloc(): corrupted top size`) and near64k (stack 8); the rest passes.

### Small leftovers, 5: B3, BASIC input at the end of stdin; B4 found

User:
> yes commit (T4: `73e4c602e`, `466f6b96d`)
> start with B3

- Reproduced in the emulator (scratch clone at HEAD), with stdin `/dev/null`, closed
  (`<&-`) and text:
  - `INPUT A`, `INPUT A$`: error 2 "Out of data" at the end, error 29 "Bad file mode"
    with stdin closed (`__fillbuf` sets `_IOERR` when `read()` fails): already fine.
    The Tasks.md text blamed `_readint`/`_readflt`; wrong.
  - `LINE INPUT A$`: `_rdline` (`lang/basic/lib/read.c`) called itself after
    "?Redo" whenever `fgets()` returned NULL: at the end, endless recursion until
    the stack reached the program ("write into text or rom"); closed: endless
    "?Redo".
- Fixed: on the terminal (`_chann == -1`) `_rdline` gives error 29 if `ferror`, else
  error 2, as INPUT does; other channels keep error 40. A first version gave 29/2
  on every channel; test28 (LINE #1 past the end of a file opened "r", which is
  `fopen(.., "ab")`, so `ferror`) then printed error 29 instead of the expected 40;
  kept 40 there.
- After the fix: LINE INPUT gives error 2 / 29 like INPUT; full build, all tests pass.
- **B4 found:** with `ON ERROR GOTO 100`, every error (also a plain `ERROR 5`) ends
  in an address error at 0x9c0001. `prolog2()` in `lang/basic/src/gencode.c` calls
  `__setjmp(env, savemask)` with the return address pushed first (as
  `cemcom.ansi/eval.c` does for C) but no `savemask`; `__setjmp`
  (`libcc.ansi/core/setjmp/setjmp.e`) takes the return address from
  `PSIZE+WSIZE`. Tried in the scratch clone only (no go-ahead yet): `C_loc(0)`
  before `env` and `asp` 2 pointers + 1 int; `ERROR 5`, INPUT and LINE INPUT at the
  end and with stdin closed all print "caught <n>"; full build, all tests pass.

### Small leftovers, 5b: B4 applied, with a test

User:
> yes commit B3, then apply B4 with the test

- B3 committed (`78ab6e78d`, `b23b5389f`).
- `lang/basic/src/gencode.c`, `prolog2()`: `C_loc(0)` (savemask) between the return
  address and `env`; `asp` BEMPTRSIZE + BEMINTSIZE + BEMPTRSIZE.
- Test `basic_onerror_test`: `plat/minix68k/tests/basic/onerror.bas` (ON ERROR GOTO
  100, LINE INPUT at the end of stdin, the handler prints `err`/`erl` and raises
  error 5, which ends the program) and `onerror.b.g`. `tests/build.py` takes our own
  BASIC tests from `BASIC_OWN` next to `lang/basic/test`.
- Checked (scratch clone): full build, all tests pass. The test fails with the old
  `gencode.c` (emulator address error after "ERROR 2") and with the old `read.c`
  (address error during the LINE INPUT recursion).

### Small leftovers, 6: F9, build-system notes

User:
> yes commit (B4: `94be27f74`, `8f2be0337`)
> i pushed (CI for `8f2be0337` passed: sanitizers 94 s, build-linux 57 s, clang 83 s)
> start with F9, delete drone and travis

- Deleted `.drone.yml` (teaci/msys32 image, `make +ack`) and `.travis.yml` (Ubuntu
  trusty, macOS, `make +ack`): the `+ack` target no longer exists; CI is
  `.github/workflows/ccpp.yml`.
- `Makefile`: `PLATS ?= minix68k` instead of `$(error ...)` for a command-line
  `PLATS`. Upstream added the error in `9a9f8ee1d` (2025, "variables set on the
  command line don't trigger a build.mk regeneration"); `build/ab.mk` now writes
  every make variable (command-line ones included) to `$(OBJ)/vars.txt` and removes
  `build.ninja` when that changes. README: one line that PREFIX and PLATS can be
  given on the command line.
- `OBJ`: `build/_sandbox.py` (vendored from ab) links inputs into
  `join(sandbox, path)` and runs commands there with relative paths, so `OBJ` must be
  a relative path inside the tree. Not patched (it is ab's design); the `Makefile`
  stops with "OBJ=...: must be a relative path inside the source tree" for an
  absolute path, `~` or any `..`.
- Checked (scratch clone): `OBJ=/abs/obj`, `../obj`, `a/../b`, `~/obj` each stop
  with the message; default `make` builds and tests minix68k; `make PLATS="minix68k
  pc86"` regenerates and builds pc86 too; `make` again regenerates without pc86 (0
  pc86 targets in build.ninja); a further `make`: no regeneration, "no work to do".
