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
