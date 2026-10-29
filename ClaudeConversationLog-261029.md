# Claude conversation log — 2026-10-29

Continues `ClaudeConversationLog-261024.md`. Open work is in `Tasks.md`.

## Session 3

### Request

> look at the current code in this directory, do not look outside this directory.
> inspect the code and see if there are obvious errors in the code; split the
> conversation log in daily parts like ClaudeConversationLog-YYMMDD.md; if you find
> issues write them in a Tasks.md file

### Repository state

- New since 2026-10-24: merge `c50619962` ("sync from upstream"), which only merges the
  empty commit `b59a37d7f` ("Update README", used to re-trigger CI); the tree is the
  same as `96e02d094`. The merge's author is the placeholder
  `Your Name <you@example.com>` (Tasks N6).
- Findings F1–F11 from the earlier sessions are all still present (no code changes since).

### Log split

`ClaudeConversationLog.md` → `ClaudeConversationLog-261023.md` (Session 1 and its
continuation, `git mv` keeps the history) and `ClaudeConversationLog-261024.md`
(Session 2: action pins, first CI run, F11, Linux only). Content unchanged apart from
a header per file and one cross-reference to the other file.

### Code check

Method: a fresh clone in the scratchpad, `make +all` (minix68k) with gcc and
`-Wall -Wextra` (minus the noisy categories: unused, sign-compare, implicit
fallthrough, parentheses, missing field initializers): builds fine, 202 warnings.
The high-signal ones (array bounds, format, sizeof in strncpy, dangling pointer,
misleading indentation, multi-statement macros, type limits, maybe-uninitialized,
strict aliasing, char subscripts) were each checked in the source.

Real issues (details and fixes in `Tasks.md`):

- **N1** `util/cmisc/tabgen.c:353`: `getln()` writes `s[-2]` when a line fills the
  buffer (`n` is -1 after `while (n--)`).
- **N2** `lang/cem/cemcom.ansi/stab.c:217`: `int` printed with `%ld` in `-g` stabs
  output; more `%ld`/`%lx` with `uint32_t` in `util/amisc` (output only).
- **N3** non-literal format strings: `util/arch/archiver.c:476`,
  `lang/cem/cpp.ansi/error.c:33`, `lang/basic/src/basic.g:242`, `basic.lex:247`.
- **N4** fragile but currently correct: `cv()` macro in `lang/basic/src/func.c:20`,
  indentation in `parsepar.c`, negative shift in `util/opt/peephole.c:266`.
- **N5** signed `char` array indexes in the C lexer and cpp (only digits today).
- **N7** the strict-aliasing problem of F11 is not only in Modula-2:
  `modules/src/alloc/alloc.h:27` (`_A_st_free`) is type-punned and used by the C front
  end (66 warnings in `lang/cem`). Recommendation: `-fno-strict-aliasing` in `CFLAGS`.

Checked and harmless: listed at the end of `Tasks.md`.

The code check itself changed no source files.

### F2 fixed: `cv` relocation table overflow

User:
> start with the cv overflow fix

- `plat/minix68k/cv/cv.c`, `emit_relo()`: the table is no longer built in a buffer of
  `4 * nrelo` bytes (too small when relocations are far apart: one extra byte per 254
  bytes of gap; and `*((long*)b) = curr` stored 8 bytes on a 64-bit host). It is now
  written through a 512-byte buffer with `relo_put1()`/`relo_put4()` (big-endian),
  `cvlong()` is gone, and `write()` errors are fatal. Input the GEMDOS format cannot
  express is now a `fatal()` instead of a silently wrong table: a relocation at an odd
  address, a first relocation at address 0 (a start value of 0 means "none"), and
  duplicates or out-of-order relocations (a 0 step byte would end the table early).
  `calloc()` asks for at least one element so 0 relocations is not "No memory".
- Tests (scratch clone, gcc, `ack -mminix68k -O -c.out` then `cv`):
  - before: the F2 program (three pointers separated by two 30000-byte initialised
    arrays) aborts with `malloc(): corrupted top size`;
  - after: it converts (43 relocations, 236 skip bytes, 283 table bytes against the
    old 4*43 = 172-byte buffer);
  - 7 other programs (empty `main`, hello with pointer tables, `hilo.c`, `hilo.bas`,
    `mandelbrot.c`, `paranoia.c`, `startrek.c`): output **byte-identical** to the old
    `cv`;
  - all 8 identical to the old algorithm compiled with a 64 KB larger buffer (so the
    F2 output matches what the old code meant to write);
  - the new `cv` under ASan + UBSan: no reports; the two `write()` warnings are gone;
  - decoding the F2 table: every relocated long points into the image (three point at
    exactly the end of bss: `.reghp`, `.limhp` in `boot.s` and one more end pointer).

### Testing

User:
> while you are at it, suggest as many tests as needed to verify things work, not
> sure if this software suite has a testing suite

ACK has `tests/plat` (about 60 self-checking programs) run per platform on an
emulator, but minix68k has no emulator and is not in `TEST_PLATS`, so nothing runs for
it. `lang/basic/test` has 47 BASIC programs with expected output, unused. The plan is
in `Tasks.md`, "Tests to add" (T1–T9); the key piece is a minix68k emulator modelled on
`plat/linux68k/emu`, with Musashi in 68010 mode and address errors on.
