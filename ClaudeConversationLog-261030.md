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

### F1b, 1: soft-float rounding (investigation; fix tried in a scratch clone)

User:
> i pushed (CI for `60825654f` passed: clang 85 s, build-linux 86 s, sanitizers 104 s)
> start with F1b

- Only `mach/m68k2/libem` uses `mach/proto/fp` in this fork. C programs ignore the
  float overflow/underflow traps (`head_ac.e`: `sim` 1336), so overflow gives the
  bit pattern of infinity without a trap.
- Differential test (scratch, TestFloat-style): host program `gen.c` writes cases
  `op a b result` (double and float `+ - * /`, double→float, long→double; random
  bits, mantissas with many trailing zeros or ones for exact ties, close and far
  exponents; a second mode with subnormal operands and results), the result from
  host IEEE arithmetic; `check.c`, built with ACK for minix68k, recomputes them in
  the emulator (about 3,700 cases/s) and prints every difference.
- Old code: 18 of 20,000, 56 of 100,000 normal cases, 14 of 30,000 subnormal cases
  differ, every one 1 ulp too low (an above-half case rounded as a tie, to even).
  Causes: no sticky bit anywhere.
  - `compact()` float: rounding looked only at `m1`, not `m2` (seen in double→float);
  - `sft_ext()` (alignment for `+`/`-`) and the subnormal path in `compact()`
    (`b64_rsft` + `b64_sft`) dropped the bits shifted out;
  - `mul_ext()` rounded the 128-bit product at bit 64 (half up) and dropped the
    rest: two roundings;
  - `div_ext()` (bit loop, `USE_DIVIDE` not defined) dropped the remainder.
- Fix tried (scratch clone only): `b64_jsft()` in `shifter.c`, a right shift that ORs
  the lost bits into bit 0 (renamed `.b64_jsft` by `massage_s.lua`), used in
  `sft_ext()` and for subnormals in `compact()`; `compact()` float rounding counts
  `m2 != 0`; `mul_ext()` ORs the low 64 bits of the product into bit 0 instead of
  rounding; `div_ext()` sets bit 0 when 64 quotient bits leave a remainder. At least
  11 (double) or 40 (float) bits lie below the result, so one sticky bit is enough.
- Result: 0 of 20,000 + 100,000 normal and 0 of 30,000 subnormal cases differ; full
  build, all tests pass. Paranoia: "Multiplication/Division/Addition/Subtraction
  appears to round correctly", "Sticky bit apparently used correctly"; FLAWs 2 → 1.
- The remaining FLAW, "unbalanced range; UfThold * V = 4.000000000000001": Paranoia's
  `V` is `7ff0000000000000`, infinity (host: `7fefffffffffffff`). Overflow already
  gives infinity, but `extend()` treats exponent 2047 as an ordinary number
  (2^1024), so arithmetic on infinity gives finite results and Paranoia's search
  for the overflow threshold ends at infinity. NaN is not known either, and
  `extend()` turns -0 into +0. `printf` prints infinity as 1.797693134862316e+308.

### F1b, 2: step 1 applied (rounding), with a test

User:
> yes apply step 1

- The scratch fix applied unchanged to `mach/proto/fp` (`shifter.c`, `FP_types.h`,
  `massage_s.lua`, `sft_ext.c`, `compact.c`, `mul_ext.c`, `div_ext.c`).
- Test `fp_test` (`plat/minix68k/tests`): `fpgen.c` (host program, the scratch
  generator), `fpcheck.c` (minix68k, the scratch checker), `fpcheck.sh` runs seed 1
  with 20,000 normal cases and seed 2 with 10,000 subnormal cases (about 8 s) and
  fails on any difference, printing the first 20. `paranoia.sh`: `MAX_FLAWS` 1.
- Checked: fresh clone, full build: all tests pass (`fp normal: 20000 cases, 0
  differ`, `fp subnormal: 10000 cases, 0 differ`). With the old `mach/proto/fp`:
  `fp_test` fails (18 differ) and `paranoia_test` fails (flaws=2, max 1). `fpgen`
  with ASan+UBSan (strict) and with clang: no reports, identical output.

### F1b, 3: step 2, infinity, NaN, -0 (tried in a scratch clone)

User:
> i pushed (CI for `aa22b0173` passed: clang 91 s, build-linux 92 s, sanitizers 126 s)
> start with step 2

- Representation: `EXT_INFNAN` (32767) as the EXTEND exponent; mantissa `NORMBIT`
  for infinity, more bits (`EXT_QNAN` = NORMBIT + quiet bit) for NaN, so the
  existing "mantissa 0 = zero" tests stay right. `EXT_ISNAN`/`EXT_ISINF`/
  `EXT_ISZERO` in `FP_bias.h`.
- `extend()`: exponent 2047/255 → `EXT_INFNAN`, NaNs made quiet; zeros keep their
  sign. `compact()`: `EXT_INFNAN` → exponent all ones (a NaN keeps its top fraction
  bits); zero written with its sign (was `zrf8`, always +0).
- `add_ext()`: NaN in → NaN; inf + finite = inf; inf - inf = NaN; -0 + +0 = +0;
  exact cancellation gives +0. `sub_ext()` is now "negate e2 and add" (the sign kept
  as `0x8000`, as `extend()` sets it: `mul`/`div` combine signs with XOR).
  `mul_ext()`: NaN; inf * 0 = NaN, inf * x = inf; 0 * x = signed 0; overflow in
  EXTEND gives inf (was a zero mantissa, i.e. 0). `div_ext()`: NaN; inf/inf = NaN,
  inf/x = inf, x/inf = 0, 0/0 = NaN (no trap), x/0 traps EFDIVZ as before and then
  gives inf (was 0); the sign XOR moved to the top.
- `adf4`/`adf8`: their shortcuts for a +0 operand returned the other operand,
  wrong for -0 (-0 + +0 must be +0): removed. `ngf4`/`ngf8`: negate 0 too. `cmf4`/
  `cmf8`: a NaN compares unequal to everything (EM's `cmf` has no "unordered", so
  < or > holds for it). `fef4`/`fef8` (frexp): inf and NaN unchanged.
- Bugs found on the way: (1) my first `sub_ext()` flipped the sign to 1, not
  0x8000: 906 of 20,000 cases wrong; (2) in `extend()` bit 31 of the shifted
  mantissa still holds the lowest exponent bit, so every infinity became a NaN
  (`2 * inf` gave NaN; Paranoia's overflow search then ended at 2^-32: a SERIOUS
  DEFECT); (3) old, in `cmf8()`: with opposite signs only the high words were
  checked for zero, so the smallest subnormal compared below -0.
- `fpgen.c`: mode `s`, operands often ±0, ±inf, NaN, the largest/smallest numbers
  or large enough to overflow; also `f2d`, negation (`n`, `fn`) and comparison (`c`,
  `fc`: a hex digit with bits for <, ==, >, !=). `fpcheck.c`: any NaN matches any
  NaN; with a NaN only == and != are checked. `fpcheck.sh`: third set, seed 3, 10,000.
- Results: normal 20,000 + 100,000, subnormal 10,000, special 11,704 + 93,485
  cases: 0 differ; full build, all tests pass. Old code on the special set: trap 7
  (0/0) ends the program, 20 differences before that. Paranoia: "No failures,
  defects nor flaws have been discovered. The arithmetic diagnosed seems
  Satisfactory." (host: "Excellent"; what remains is step 3: `sqrt` rounding,
  `pow(0,0)`, `printf` of infinity, "Overflow saturates at V0 = 2.3e-10").
- Not covered by tests: `fif` (modf) and `cfi`/`cfu` with infinity or NaN (they
  take their existing overflow paths).

### F1b, 4: step 2 applied

User:
> yes apply step 2

- The scratch diff applied unchanged (15 files in `mach/proto/fp`, `fpgen.c`,
  `fpcheck.c`, `fpcheck.sh`); `paranoia.sh`: `MAX_FLAWS` 0.
- Checked: fresh clone, full build: all tests pass (fp normal 20,000, subnormal
  10,000, special 11,704: 0 differ; Paranoia with no FLAW allowed). `fpgen` with
  ASan+UBSan (strict) and with clang: no reports, identical output in all 3 modes.

### F1b, 5: step 3, the C library (tried in a scratch clone)

User:
> i pushed (CI for `a783ea141` passed: clang 83 s, build-linux 90 s, sanitizers 197 s)
> start with step 3

- `printf` (`lang/cem/libcc.ansi/core/printf/doprnt.c`): numbers are rendered into
  `static char buf[16]` (upstream `53a3630d2`, 2020: was `buf[1025]` on the stack,
  too much for CP/M), and float precision was cut to 15 to "fit". But `%f` of 1e30
  is 38 characters, of `DBL_MAX` 316, `%.15e` 23, `%lb` 32, `%.30d` 30: all written
  past the buffer. Seen in the emulator: `printf("%f", DBL_MAX)` overwrote a
  pointer with "000" and the program jumped to 0x303030. All platforms.
  Fix: `buf[LDBL_MAX_10_EXP + 4 + MAXPREC]` (static, 440 bytes with 8-byte
  doubles), precision cut to `MAXPREC` = 128 for floats and integers. `%.17e` now
  gives 18 digits.
- `fltpr.c`: infinity and NaN printed as `inf`/`-inf`/`nan` (`INF`/`NAN` for `%E`,
  `%G`), with `+`/space flags; was digits of a huge number (`2.3e-10` for inf).
- `pow.c`: `y == 0` first, so `pow(x, 0)` = 1 for every x, 0 included (C99/IEEE;
  was 0 with EDOM). `pow(0, y < 0)` still EDOM, 0.
- `sqrt.c`: Newton's method (5 steps with a division) replaced by an exact integer
  square root: `frexp`/`ldexp` give x = M * 2^k (M an integer of 53/54 bits, k
  even), the root of M * 2^52 is computed two bits at a time in pairs of 32-bit
  words (53 steps), rounded up when the remainder exceeds the root (never a tie).
  No byte order assumed; `sqrt(-0)` is -0 now. Host check of the same code against
  glibc: 0 of 4,000,000 random (incl. subnormal) and 600,000 near perfect squares
  differ.
- Tests: `fpgen`/`fpcheck` ops `sq` (sqrt, exact) and `rt` (printf `%.16e`, then
  strtod: the same double); about 5,000 such cases over the 3 sets. `paranoia.sh`
  also requires the verdict "Excellent" (catches its warnings).
- Results: all tests pass; Paranoia: "Square root appears to be correctly rounded",
  no pow warning, "Overflow saturates at V0 = inf", "The arithmetic diagnosed
  appears to be Excellent!" (as the host). With the old C library: fp normal 1,172,
  subnormal 339, special 106 differ (sqrt last bit; round trip with 16 digits),
  Paranoia `excellent=0`. `make PLATS="minix68k linux68k pc86"`: all pass (47
  linux68k tests).
- Measured, not fixed: `%.16e` (17 significant digits) differs from glibc in the
  last digit in 12 of 5,000 random doubles (`_ext_str_cvt` works with 64-bit
  extended precision); the round trip `printf`→`strtod` still gives the same
  double in all 5,000, and ACK's `strtod` reads glibc's exact text right. Beyond 17
  digits (`%.20e`, `%f` of big numbers) ACK prints zeros or approximate digits
  (C only recommends exactness up to `DECIMAL_DIG`); an exact decimal tie
  (`...124.8125` to 3 places) is rounded up, glibc to even.

### F1b, 6: step 3 applied; F1b done

User:
> yes apply step 3

- The scratch diff applied unchanged (`pow.c`, `sqrt.c`, `doprnt.c`, `fltpr.c`,
  `fpgen.c`, `fpcheck.c`, `paranoia.sh`).
- Checked: fresh clone, full build: all tests pass (fp normal 20,000, subnormal
  10,000, special 11,601: 0 differ; Paranoia "Excellent"). `fpgen` with ASan+UBSan
  and with clang: no reports, identical output in all 3 modes.
- `Tasks.md`: F1b done; new F1c (17th printf digit, see step 3).

### Upstream branches

User:
> i pushed (CI for `14fca3372` passed: build-linux 109 s, sanitizers 259 s, clang 104 s)
> start with the upstream branches
> leave out the minix68k it is for version1 which i expect nobody is using, we can
> always point to my fork for interested parties
> keep it, reword the message (tests-plat-wordsize)
> yes fetch the branches
> yes put the PR texts in the log

- Upstream `default` (davidgiven/ack, `git ls-remote`) is still `7afa32a0a`, this
  branch's fork point. Upstream has `plat/minix68k` and `mach/m68k2/libem`, but no
  `mach/proto/fp` build and no minix68k tests; its tests run on cpm, linux68k,
  linuxppc, pc86. Decision: only generic fixes go upstream; the minix68k series
  stays in the fork (memory: upstream-scope).
- Branches made in a scratch clone by cherry-pick (no `-x`: its line would follow
  the Co-Authored-By trailer), then fetched into this repo:
  `upstream/libc-fixes` (889c16e4f, f39e6910a), `upstream/ego-fixes` (34019b8ff,
  76abdb282), `upstream/ub-fixes` (a648a2838, b5c8d7cbe, a8c7ee66e, 80fc1fa37
  without its `basic.g` line, 47dc594be, ddb15dd2c, 6ba184220, 01f77a5d4),
  `upstream/basic-fixes` (4c4cafd9a, b9831450b, 8a2103d9b, 78ab6e78d, 94be27f74,
  without their `plat/minix68k/tests` parts; the DATA commit takes the `"%s"` fix
  for its rewritten line and says so; the ON ERROR commit shows a 3-line BASIC
  example instead of the fork's test), `upstream/tests-plat-wordsize` (1159d464e,
  message reworded without minix68k). All five merge together without conflicts
  and contain no fork-only files.
- Full upstream builds (`PLATS = all` and upstream's tests): the first attempt of
  the baseline and the combined build in parallel ran out of `/tmp` inodes; rerun
  one after the other: the baseline alone used all 1,048,576 inodes, so a full
  `PLATS = all` build does not fit in this `/tmp`. Checked instead with the
  platforms upstream tests (`PLATS = cpm linux68k linuxppc pc86` in the clone's
  Makefile): plain `default` and `default` with all five branches merged both pass
  all 193 tests (200 s / 202 s, about 517,000 inodes each).
- PR texts (for the user to open the PRs):

#### Upstream PRs (davidgiven/ack, base `default`)

Each branch is based on `default` at 7afa32a0a and is independent; all five
merge together without conflicts. Found while making the minix68k target
usable in my fork (https://github.com/maarten-boot/ack, branch
`rosco-minix68k`); only the generic fixes are offered here.


#### upstream/libc-fixes — libcc.ansi: printf buffer overflow, exact sqrt, pow(x, 0), inf/nan, fflush

- **printf overflowed its number buffer.** Since 53a3630d2 numbers are
  rendered into `static char buf[16]`, and float precision is cut to 15 to
  fit. But `%f` of DBL_MAX is 316 characters, `%f` of 1e30 38, `%.15e` 23,
  `%lb` 32, `%.30d` 30: all written past the buffer. `printf("%f",
  DBL_MAX)` crashed a program. The buffer now has room for `%f` of the
  largest long double with 128 digits after the point (static, about 440
  bytes with 8-byte doubles, so still nothing on the stack); precision is
  cut to 128 for floats and integers. `%.17e` gives the 18 digits asked for.
- printf prints infinity and NaN as `inf`, `-inf`, `nan` (`INF`/`NAN` for
  `%E`/`%G`).
- `pow(x, 0)` is 1 for every x, 0 included (C99, IEEE 754).
- `sqrt` is correctly rounded: an exact integer square root of the mantissa
  (two bits at a time, in pairs of 32-bit words, no byte order assumed)
  replaces Newton's method (errors of -0.60 to +0.50 ulp). Checked against
  glibc: 0 differences in 4,000,000 random values (subnormals included) and
  600,000 values at and next to perfect squares.
- `fflush()` on a stream being read must give the unread buffered bytes
  back to the file, but seeked forward over them instead of back: they were
  skipped (since d87444a7f, 1990). The BASIC runtime flushes before every
  READ, so with DATA in a file every second DATA line was lost.

With these (and correct soft-float arithmetic in my fork) Kahan's Paranoia
says "Excellent", as on the host.

#### upstream/ego-fixes — ego: inliner lost functions at -O4; cs NULL dereference

- **il:** `cl_flags` holds bit flags (CLF_SELECTED 020, CLF_EVER_EXPANDED,
  CLF_FIRM, the parameter count) but was declared `bool`; since "Use stdbool
  throughout" (0782bc80e) that is a `_Bool`, so `SELECTED()` stored 1 and
  `IS_SELECTED()` never matched. `cleancals()` then dropped every selected
  call after `singles()` had marked the callee for removal: at -O4 and up a
  static function called once lost its body while the call stayed
  ("Undefined: __II0" at link time). Now `byte`.
- **cs:** `gains()` looked at the instruction before an "addressing mode"
  expression; when the expression starts a basic block that is NULL (seen
  with `adp 1` in tests/plat/core/exg). It is now not eliminated.

#### upstream/ub-fixes — undefined behaviour and robustness fixes (host tools)

Found with ASan/UBSan and clang builds of the host tools running the tests:

- em_opt looked up pseudo-instructions in `em_flag[]`, which only covers
  instructions (read 25 bytes past the table; whether `sp_cend` was written
  depended on the bytes after it);
- signed shifts that are undefined (LLgen sets, both `arith.h`, the
  `cstoper.c` files, peephole, fillem, cset, Musashi);
- tabgen's `getln()` wrote before its buffer on a long line;
- printf formats: argument types, and text passed as the format (a `%` in a
  name or source line was read as a conversion);
- a two-statement macro, misleading indentation, negative shifts;
- tables indexed with (signed) `char`;
- m2: two `!a & b` meant as `!(a & b)` (a missed "statement not reached");
- Makefile: build the host tools with `-fno-strict-aliasing`: the C front
  end type-puns its free lists (modules/src/alloc/alloc.h) and the Modula-2
  front end its set reference counts (lang/m2/comp/chk_expr.h), which
  clang -Os already miscompiles.

#### upstream/basic-fixes — ACK BASIC: ON ERROR, LINE INPUT, ^, DATA

- **ON ERROR GOTO jumped to a garbage address:** the program prologue called
  `__setjmp(env, savemask)` without `savemask`, so the return address was
  read from the wrong place.
- **LINE INPUT at the end of stdin** recursed (with "?Redo") until the stack
  was gone; now error 2 / 29 like INPUT.
- **`^` computed exponent ^ base** since the 2013 rewrite (`n ^ .5` gave
  `.5 ^ n`).
- **DATA kept in the program:** the compiler wrote DATA to `<source>.bas.d`
  and every program opened that file at start-up by its compile-time name, so
  a BASIC program only ran where it was compiled (examples/hilo.bas: "File
  not found"). Now the DATA text is emitted as a rom string and READ/RESTORE
  read it there.
- The DATA file name was emitted as a pointer instead of its length.

#### upstream/tests-plat-wordsize — tests/plat: word-size and byte-order clean

aar, cii, inn and rck assumed EM_WSIZE == EM_PSIZE or a little-endian
machine (pointer compares with the word size, expected values built for
little-endian, a 4-byte argument to fail()). Results unchanged on
linux68k, pc86, cpm and linuxppc.


🤖 Generated with [Claude Code](https://claude.com/claude-code)

### P2: the minix project builds this fork (proposal; checked in a scratch copy)

User:
> start with P2

Read-only look at `../minix-st-2.0.4/src/rosco/ack` (nothing changed there):
`build-ack.sh` clones davidgiven/ack at `7afa32a0a` and applies
`ack-minix68k.patch`; `Makefile` (host, lib, pass, ccproto) uses
`ack/.obj/staging`, `mkpass.py` builds the passes as Minix programs from the target
paths in `ack/.obj/build.ninja`; `acc-minix` links `crtso.o emrts.o <objects>
libc.a libem.a libend.a` (Minix's C library, ACK's libem/libend).

- The patch is all in this fork: `mach/m68020/ncg/mach.c`, `mach/m68k2/as/mach1.c`,
  `mach5.c`, `plat/minix68k/cv/cv.c` identical (`git apply -R --check`);
  `Makefile` (PLATS/DEFAULT_PLATFORM) and `maketempfile.c` (Minix, plus F7) in
  another form. The fork changed no target paths `mkpass.py` uses.
- Upstream's m68k2 libem has `fp68881.s`: float code on the board would run 68881
  instructions (F-line trap on the 68010). The fork's libem has the soft-float.
- Proposed change (for the project's owner), `src/rosco/ack`:
  - `build-ack.sh`: `ACK_URL` https://github.com/maarten-boot/ack.git, `ACK_REV`
    `14fca33720e16be2ffb1509cd35c90e12b2a30f2` (tag minix68k-20261030), no
    `git apply`; header comment: what the fork includes, that `make` also runs the
    minix68k tests (a few minutes, coreutils `timeout`), remove `ack/` to change
    revision;
  - `Makefile`: `$(STAGING)/bin/ack: build-ack.sh` (no patch dependency);
  - delete `ack-minix68k.patch`.
- Checked in a scratch copy (`include`, `src/lib`, `src/rosco/{lib,include,ack}`,
  the `src/{fs,mm,kernel}` headers): the changed `build-ack.sh` with `ACK_URL` =
  this repository builds the fork; `make -C lib` builds Minix 2.0.4's `libc.a`,
  `crtso.o`, `emrts.o` without errors; a float program linked as `acc-minix` does
  (plus the pieces below) runs in this fork's emulator and prints the same as the
  host (1/3, a sum of 1000 terms, 0.1+0.2 == 0.3).
- Found in that check:
  - **cpp.ansi overflow (this repo, all platforms):** `preprocess.c` writes `#line`
    directives into `char Xbuf[256]` with `sprintf`; a source or header path of
    about 240+ characters aborts the preprocessor ("buffer overflow detected"; the
    scratch copy's long paths made 4 `cv_test` cases fail). `replace.c`'s
    `FilNamBuf[PATHLENGTH]` for `__FILE__` lacks room for the two quotes and the NUL.
    Not fixed yet (needs a go-ahead).
  - **minix project:** its `lib/Makefile` puts only the stubs `fphook/fltpr.o` and
    `strtod.o` ("cannot print floating point") into `libc.a` and builds no float
    library from `fphook/fphook.c`, and leaves out `src/lib/math` (`ldexp`,
    `hugeval`, `isnan`; its `frexp.s` is i86/i386 only, ACK's `frexp.e` works): no
    program on the board can printf a float today, whichever ACK. Its `ncrtso.s`
    does not ignore the float traps as ACK's C start-up does (`sim` 1336), so a
    float overflow ends the program ("trap 4") instead of giving infinity.

### A container for the ACK

User:
> for compiling the minix and the bbc code we could host the ack in a docker
> container, minix itself will add the ack c compiler to its tree for development
> inside minix, am i right ?
> yes write the Dockerfile

- Two uses: the cross compiler on Linux (bbcbasic needs only the staging tree; the
  minix project's `mkpass.py` also needs the source and build tree with
  `build.ninja`), and the passes built as Minix programs (by `m68k-elf-gcc -mshort`
  in the rosco toolchain container) that the minix project puts on the SD card,
  linked with Minix's libc and this fork's libem/libend.
- `plat/minix68k/docker/Dockerfile`: `debian:trixie-slim` pinned by digest
  (`sha256:f6b08b09…`, 2026-10-27); bison, flex, gcc, git, lua5.4, make,
  ninja-build, python3; clones `ACK_URL` (default this fork) at `ACK_REV` (default
  tag `minix68k-20261030`), `make` (builds and runs the minix68k tests), keeps
  `/opt/ack` whole; `ACKDIR` and `PATH` set, `minix68k-emu` on the path, workdir
  `/src`. Committed before the first test build finished (user's request).
- First test build: `git clone` in the container fails, "server verification failed:
  certificate signer not trusted": this network inspects TLS (the host has
  `PA-Root-CA.crt` and `forward-trust-*.crt` in `/usr/local/share/ca-certificates`,
  the container does not). Not a Dockerfile fault; a normal network or GitHub's
  runners clone fine.
- Tested with a copy of the Dockerfile that copies a local checkout at
  `minix68k-20261030` instead of cloning: build and minix68k tests in 111 s; run as
  the user with a mounted directory, `ack -mminix68k` compiles C (`sqrt(2) =
  1.414213562373095`) and BASIC (`2^10` = 1024), both run in `minix68k-emu`, files
  owned by the user; `ACKDIR`, `build.ninja` in place. Image 1.13 GB (the whole
  build tree).
- User: "i stopped the proxy stry again". The committed Dockerfile unchanged: clone
  from GitHub at `minix68k-20261030`, build and 95 tests in 105 s; the image
  compiles and runs a C program (`0.1 + 0.2` = 0.30000000000000004).
- Found with that program: `printf("%.17g", 0.1 + 0.2)` gave 0.29999999999999999.
  The run-time soft-float is right (`a + b` with volatile operands:
  `3fd3333333333334`, printed 0.30000000000000004, as the host); the constant
  `0.1 + 0.2` folded by the compiler is `3fd3333333333333`, 1 ulp low (gcc folds to
  `...334`). ACK's compile-time float arithmetic is not IEEE-rounded: new F1d.
- `git -C /opt/ack describe` as a non-root user: "detected dubious ownership"
  (the tree belongs to root). Harmless for compiling.
- User: "yes add the safe.directory line and commit". Dockerfile: `git config
  --system --add safe.directory /opt/ack`. Rebuilt (127 s); `git -C /opt/ack
  describe --tags` as the user: `minix68k-20261030`.
