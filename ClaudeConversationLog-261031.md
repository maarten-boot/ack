# Claude conversation log — 2026-10-31

Continues `ClaudeConversationLog-261030.md` (the session ran past midnight).

- Both branches pushed by the user: `rosco-minix68k` at `a57724f1d`,
  `upstream/codegen-fixes` at `7eb551ec9`. PR davidgiven/ack#383 ("Code
  generation fixes found by differential testing") is open, next to #379-#382.
- User: "continue". T7 marked done in `Tasks.md` (all steps finished, PR open).
- User pasted a CI failure (sanitizer job, `diff_test`): ASan
  heap-use-after-free in `eqtoken` (`mach/proto/ncg/subr.c:282`), seeds 22
  (-O2) and 54 (-O0, -O), "compile error". All other tests passed (it was
  the last target). The job had not run `diff_test` under ASan before.
  - Reproduced in scratch: minix68k `ncg` built with the job's sanitizer
    flags. Free in `garbage_collect()` (`salloc.c:114`), read in `test()`
    (`move.c:109`) comparing with `cocoreg` = `machregs[0].r_contents`, the
    condition-code pseudo register that `setcc()` fills (instructions with
    `sets cc`, DO_SETCC).
  - Cause: `garbage_collect()` marks the strings still in use by scanning
    the EM lines, the fake stack and the registers from `machregs+1`, so a
    string only in the condition-code register is freed; `test()` then
    `strcmp`s freed memory. Wrong code is possible if the freed memory is
    reused (a test wrongly skipped as already done). Original code, same in
    upstream `7afa32a0a`; affects every ncg back end whose table uses `sets
    cc` (m68020, i86, i386, i80, pdp, powerpc, arm).
  - Fix tried in scratch (`salloc.c`): scan from `machregs`, skipping
    `machregs[0]` when it holds a register (`t_token == -1`). With it seeds
    22 and 54 compile cleanly under ASan, and all 65 `diff_test` seeds pass
    with the ASan ncg (detect_leaks=0, as in the job).
  - User: "yes apply". Fix in `salloc.c`, `Tasks.md` S3. Committed at the
    user's request ("first commit") while the local full sanitizer build
    of the working tree was still running.
  - GitHub: `build-linux-sanitizers` has failed on every push since the
    `diff_test` commit (`7ed65fac2`, 12:58), push and pull_request runs;
    `build-linux` and `build-linux-clang` pass. In each run the only
    failures are diff seeds 22 (-O2) and 54 (-O0, -O), the S3 bug. Earlier
    runs (up to 12:43) passed.
  - Local full build of the working tree with the fix (scratch copy), with
    the job's flags (`CC=$SAN HOSTCC=$SAN`, `ASAN_OPTIONS=detect_leaks=0`):
    rc 0, all 112 tests pass, `diff_test` included, no sanitizer report.
- Pushed by the user; CI for `ec2730fa1` green (push and pull_request,
  all three jobs).
- User asked about reading Dependabot issues: the fork has Dependabot
  alerts disabled, issues off, no `dependabot.yml`, no Dependabot PRs;
  upstream has none visible (alerts need admin). User: "no its ok, continue".
- A1 started (analysis only): `led` keeps a module in ALLOMODL as header
  (`struct outhead`, 20 bytes), section table (`struct outsect`, 20 bytes
  each), name table (`struct outname`, 16 bytes with an 8-byte pointer on a
  64-bit host), strings; the name table is at 20 + 20 * nsect, 8-aligned
  only for odd nsect. `int_align()` rounds to `sizeof(int)`. Full build
  with `-fsanitize=alignment` running in scratch to list all places.
- A1 analysis. Scratch build of HEAD with `-fsanitize=alignment` (reports
  only, no abort): 1493 reports, all in `led` (and `rd.c` writing into
  led's memory), each 8-byte-aligned type at a 4 (mod 8) address:
  `struct outname` (relocate.c, finish.c, extract.c, rd.c `rd_name`),
  `struct ar_hdr` (scan.c:141, save.c:53), `ind_t` (scan.c 327-585).
  Cause: with USEMALLOC each piece is its own malloc block (aligned), but
  offsets inside ALLOMODL are only 4-aligned: the archive magic takes
  `sizeof(int)`, `int_align()` rounds to `sizeof(int)`, and the name table
  is at 20 + 20 * nsect after the header and section table. Everything
  after the magic of an archive is then 4 off.
  - Fix tried in scratch (`util/led`: memory.h, scan.h, scan.c, save.c):
    `int_align()` rounds to `_Alignof(max_align_t)`; the magic advances by
    `int_align(sizeof(int))` (scan.c and save.c); IND_SECT, IND_NAME and
    IND_EMIT start aligned; `modulsize()` is computed from the IND_ layout
    so the pieces `direct_alloc()` and `read_modul()` allocate add up to it
    (`alloc()` rounds each ALLOMODL piece); `align()` dropped.
  - Rebuild of the same tree with the fix (same flags): 0 reports, all
    tests pass; every program led linked again (examples, tests) is
    byte-identical; archives differ only in the 2 date bytes per member.
  - Full build with the job's flags minus `-fno-sanitize=alignment`
    (ASan + UBSan incl. alignment, no recover, detect_leaks=0) with the
    fix: rc 0, all 112 tests pass, no sanitizer report.
  - User: "yes apply". `util/led` fix applied (files identical to the tested
    scratch tree), `ccpp.yml`: `-fno-sanitize=alignment` dropped and the
    comment updated; `Tasks.md`: A1 fixed, T8 note updated. Not committed.
- User: "yes make the upstream branch". From `7afa32a0a` (the led files are
  the same there; salloc.c differs only by the S3 fix), one commit each:
  `upstream/led-align` `ab20fbf49` (A1), `upstream/ncg-gc-cc` `1e3151950`
  (S3). Pushed by the user, as was `rosco-minix68k` (`f088c1498`; CI green,
  the sanitizer job now with the alignment check).
- The `upstream/led-align` push run was cancelled by GitHub, not by hand:
  upstream's `ccpp.yml` has `concurrency: group:
  environment-${{ github.head_ref }}`, `cancel-in-progress: true`;
  `head_ref` is empty on push, so all push runs share one group and the
  `upstream/ncg-gc-cc` run, started in the same second, cancelled it
  (annotation "Canceling since a higher priority waiting request for
  environment- exists"). Re-run after the other run has finished
  (`gh run rerun 41223452353`). Earlier: `upstream/codegen-fixes` push run
  failed in build-macos and build-windows only (not looked at; Linux only).
- Upstream check (scratch, `7afa32a0a`, PLATS = all, 193 tests):
  plain build passes; with both fixes passes, and every output file is
  identical except led itself and the `hilo_bas_*` examples, which differ
  because upstream's BASIC compiles are not reproducible (N8, fixed in the
  fork in `4c4cafd9a`; two compiles of hilo.bas in the same tree differ). With `-fsanitize=alignment`: no reports in led or
  rd.c; 948 remain, all in `util/ass` (em22's assembler: struct line_t,
  relc_t, freeblock, locl_t, stat_t at 4 (mod 8) addresses; its own
  allocator in asscm.c). Not used by minix68k: new task A3 (low).
- PR text, `maarten-boot:upstream/led-align` → davidgiven/ack `default`:

  Title: led: keep the structs in a module aligned for the host

  > led keeps each input module in one memory piece: header, section
  > table, name table, strings, then the offsets of the section contents.
  > With USEMALLOC each piece is its own malloc block, but the offsets
  > inside it were only 4-aligned (the archive magic took `sizeof(int)`,
  > `int_align()` rounded to `sizeof(int)`, the name table followed the
  > 20-byte header and section entries). On a 64-bit host `struct outname`,
  > `struct ar_hdr` and `ind_t` need 8, so linking with an archive used
  > them at misaligned addresses: undefined behaviour, reported by
  > `-fsanitize=alignment` on every such link, and a fault on a
  > strict-alignment host.
  >
  > `int_align()` now rounds to `_Alignof(max_align_t)`, each table starts
  > aligned, and `modulsize()` is computed from that layout.
  >
  > Tested: `make` with `PLATS = all`, all tests pass; every linked program
  > is byte-identical to before (the `hilo_bas` examples differ between any
  > two builds because the BASIC compiler emits a heap address, a separate
  > bug). With `-fsanitize=alignment` led has no reports left (the
  > remaining ones are in `util/ass`).
  >
  > 🤖 Generated with [Claude Code](https://claude.com/claude-code)

- PR text, `maarten-boot:upstream/ncg-gc-cc` → davidgiven/ack `default`:

  Title: ncg: don't free strings still held in the condition-code register

  > `garbage_collect()` in `mach/proto/ncg/salloc.c` frees the strings no
  > longer referenced from the EM lines, the fake stack or the registers,
  > but it scans the registers from `machregs+1`. `machregs[0]` holds the
  > token of the last instruction that set the condition codes
  > (`setcc()`), so a string only referenced there was freed, and `test()`
  > then compared with it in `eqtoken()`. AddressSanitizer reports a
  > heap-use-after-free; without it a test can be wrongly skipped if the
  > memory has been reused. It affects every ncg back end whose table has
  > instructions that set cc (m68020, i86, i386, i80, pdp, powerpc, arm).
  > I hit it compiling random C programs for 68000 with ncg built with
  > `-fsanitize=address`.
  >
  > The fix also scans `machregs[0]`, unless it holds a register.
  >
  > Tested: `make` with `PLATS = all`, all tests pass, output unchanged.
  >
  > 🤖 Generated with [Claude Code](https://claude.com/claude-code)
- `upstream/ncg-gc-cc` push run: build-linux passes, build-macos and
  build-windows fail. Upstream `default` (`7afa32a0a`, merge of #372) fails
  the same two jobs in davidgiven/ack's own CI, so these failures are
  older than our branches. `upstream/led-align` run can now be re-run.
- User: "that went wrong one canceled one failed". The failure is the same
  on all six upstream branches (codegen-fixes, float-fold, volatile-nop,
  libc-fixes, cpp-longpath, ncg-gc-cc), whatever they change:
  build-windows stops generating `em_data`'s `spec.h`/`mnem.h`/`pseu.h`,
  build-macos stops compiling m2 `PascalIO.mod` (libm2, linux386/68k);
  build-linux passes on all. Upstream `default`'s own runs fail the same
  two jobs (its logs have expired, HTTP 410). So: the base, not the
  fixes. Not looked into further (Linux only).
- User: ".github/workflows/ccpp.yml is still using outdated gh actions and
  not pinned to hash, please update". The fork's workflows were already
  pinned; the outdated ones are upstream's, which the upstream branches
  carry. New branch `upstream/ci-actions` from `7afa32a0a` (separate PR,
  so the fix PRs stay small): checkout v7.0.1, setup-msys2 v2.33.0,
  upload-artifact v7.0.2, action-gh-release v3.0.3, each pinned to its
  commit (checked: API returns a commit; action.yml at that commit says
  node24). mknejp/delete-release-assets has only `v1` (a branch, node20):
  dropped, action-gh-release overwrites same-name assets
  (`overwrite_files`, default true, now explicit). ccpp.yml concurrency
  group fixed (`${{ github.workflow }}-${{ github.head_ref || github.ref }}`),
  the cause of the cancelled led-align run. YAML parses.
- PR text, `maarten-boot:upstream/ci-actions` → davidgiven/ack `default`:

  Title: CI: update the actions, pin them, and stop pushes cancelling each other

  > - All actions on their latest release, pinned to the commit SHA:
  >   checkout v7.0.1, setup-msys2 v2.33.0, upload-artifact v7.0.2,
  >   action-gh-release v3.0.3 (all node24; checkout v3 and
  >   action-gh-release v1 were node16).
  > - release.yml: `mknejp/delete-release-assets` has no release newer than
  >   v1 (node20); `action-gh-release` already overwrites an asset with the
  >   same name, so the step is dropped and `overwrite_files: true` is set.
  > - ccpp.yml: the concurrency group used `github.head_ref`, which is empty
  >   for push events, so pushes to two branches cancelled each other. It is
  >   now per workflow and branch.
  >
  > The macOS and Windows jobs fail on `default` before and after this
  > change (Windows in em_data's header generation, macOS in libm2's
  > PascalIO.mod); Linux passes.
  >
  > 🤖 Generated with [Claude Code](https://claude.com/claude-code)
- Pushed `rosco-minix68k` (`10126eb9a`) and `upstream/ci-actions`; the three
  runs ran side by side (no cancel). rosco-minix68k: all jobs pass.
  ci-actions: build-linux passes; build-windows (em_data spec.h, mnem.h,
  pseu.h) and build-macos (libm2 PascalIO.mod) fail exactly as on the
  other upstream branches, so the PR text's statement holds. No Node.js
  deprecation annotation; macOS warns that brew's lua@5.4 is deprecated
  (disabled 2027-08-26).
- User: "continue". A2 (analysis, scratch only):
  - Cause: `model` (mach1.c, `extern`) is defined by `comm3.c`, which
    includes the machine declarations with `extern` defined empty, so it
    starts at 0 = 68000; only `.68010` sets 1. `test68010()` then gives
    "68010 instruction (warning)", and `warning()` counts as an error and
    stops. Reproduced: `rtd`, `move ccr`, `movec`, `moves` fail without
    `.68010`. The m68k2 assembler is built only for minix68k.
  - Fix: `mach/m68k2/as/mach0.c` redefines the per-pass hook
    `machstart(x)` (empty in `comm0.h`, called at the start of each pass)
    as `(model = 1)`; `.68000` still switches back for the rest of the pass.
  - Test: `m68kcheck.sh` (m68kcheck_test) assembles `move ccr,d0; rtd #4`
    without a directive (must assemble, m68kcheck clean) and with `.68000`
    (must fail). Fails without the fix, passes with it.
  - Scratch full build with the fix: all tests pass; every assembled
    object byte-identical (only the assembler changed), as no library or
    compiler output uses 68010 instructions.
  - User: "yes apply". Fix and test applied (files identical to the tested
    scratch tree); `Tasks.md` A2 fixed. Not committed.
- User: "continue". T5 (scratch only):
  - cv header (`8625a2877`): already covered; `cvcheck.py` (cv_test)
    unpacks a 32-byte big-endian header, checks its fields and that the
    file size adds up, for several programs and the examples.
  - con_mult (`eaa025860`, `.data4` for 4-byte constants with 16-bit int):
    no test. New `tests/plat/core/initlong,e,.c`: an initialised long
    array, an array of structs mixing char/short/long, a static long and an
    int after them; checks every value. minix68k: passes; with the fix
    reverted (`.data8`) it fails from the second array element on (asserts
    at lines 0x1e-0x27). Portable C, runs on every platform.
  - User: "yes apply". Test and build.py entry (files as tested), Tasks.md
    T5 done. Not committed.
- User: "continue with N11 and Fc1, explain N9" (read as F1c).
- N11 (scratch only). cpp.ansi reads the name in `#line N "name"` with its
  escapes processed (`quoted()`), and so does cemcom.ansi in `# N "name"`,
  but cpp.ansi wrote the name back between quotes as it is (line_dir in
  preprocess.c, `__FILE__` in replace.c macro_func). A `"` or `\` in a
  file name gave a broken string; a name from #line lost its escapes.
  - Fix: `quoted_name()` in replace.c (declared in replace.str): the name
    as a string literal, `"` and `\` escaped, other control characters as
    3-digit octal; used by line_dir and `__FILE__`.
  - Test (`lang/cem/cpp.ansi/longpath.sh`): a directory `q"b\c` (line
    directives, `__FILE__`, and the output preprocessed again) and
    `#line 5 "a\"b\\c\tt"`. Fails without the fix (cpp cannot read its own
    output back), passes with it; full scratch build passes. End to end:
    ack -mminix68k on `q"b\c/t.c` with an error reports
    `".../q"b\c/t.c", line 2`, the real name.
  - Generic (cpp.ansi is shared): an upstream candidate, next to #380.
- F1c (analysis). printf's `%e`/`%f`/`%g` use `_ecvt`/`_fcvt` →
  `_ext_str_cvt()` (core/stdlib/ext_comp.c), which scales the value into
  [1, 10) with 64-bit-mantissa powers of ten and takes digits from that;
  each scaling rounds, so digit 17 can be off and digits beyond are not the
  exact expansion. Exact method: a double is m * 2^e (m < 2^53); for
  e >= 0 the integer m << e (up to 1077 bits), for e < 0 the integer
  m * 5^-e with the decimal point -e places from the right (up to about
  2550 bits, 767 significant digits for the smallest subnormal). Big
  integer in 16-bit limbs (32-bit products; minix68k has no 32x32 multiply
  in hardware), multiply by 5^6 per step, digits by dividing by 10^4;
  round half to even on the exact digits. Cost: about 160 limbs, ~1 KB of
  code in every program that prints floats, and on a 68010 thousands of
  limb operations for very large or very small numbers (fast for ordinary
  ones). Not yet written.
- N9 (explanation). `-l<name>` in ack's descr files maps to a library
  path; minix68k (F4) has `mapflag -l* LNAME={PLATFORMDIR}/lib*.a` and
  `-lm` → libc.a. linux68k, linuxppc, linux386, linuxmips, osxppc, em22
  and cpm have `mapflag -l* LNAME={PLATFORMDIR}/lib*`: `-lfoo` gives
  `.../libfoo` without `.a`, which no build installs, so led cannot open
  it; and no platform builds a separate libm (the maths functions are in
  libc), so `-lm`, the common case, fails everywhere but minix68k. Fix as
  F4: add `.a`, map `-lm` to libc.a. Upstream only (other platforms).
- User: "make a note but skip N9 and F1c". Noted in `Tasks.md` under both
  (skipped, with the fix/design). N11 waits for "yes apply".
- User: "yes apply". N11 fix and test applied (files identical to the
  tested scratch tree); `Tasks.md` N11 fixed. Not committed.
- User: "continue with N11 upstream". The fix changes code PR #380 added
  (line_dir, the `__FILE__` buffer), so `upstream/cpp-escape` is made on
  top of `upstream/cpp-longpath` (`509093957`, #380), not from
  `7afa32a0a`: commit `cb29db892`; the three files were otherwise the same
  as in the fork. Pushed by the user. Scratch check of the branch with
  PLATS = all: rc 0, 194 tests, none fail (cpp.ansi test included).
- PR text, `maarten-boot:upstream/cpp-escape` → davidgiven/ack `default`:

  Title: cpp.ansi: escape file names in line directives and __FILE__

  > Builds on #380 (its commit is included here; once #380 is merged only
  > the second commit remains).
  >
  > The name in `#line N "name"`, and in the `# N "name"` lines the
  > compilers read, is a string literal: both cpp.ansi and cemcom.ansi read
  > it with its escapes processed. But cpp.ansi wrote file names back
  > between quotes as they are, so a `"` or `\` in a file name gave a
  > broken string (cpp.ansi could not even read its own output back), and
  > a name that came through `#line` lost its escapes.
  >
  > `quoted_name()` writes the name as a string literal (`"` and `\`
  > escaped, other control characters in octal); the line directives and
  > `__FILE__` use it. With it, an error in `q"b\c/t.c` is reported for
  > that file.
  >
  > Tested: `longpath.sh` gets a directory named `q"b\c` (line directives,
  > `__FILE__`, and the output preprocessed again) and `#line` with
  > escapes; it fails without the change. `make` with `PLATS = all`: all
  > tests pass.
  >
  > 🤖 Generated with [Claude Code](https://claude.com/claude-code)

## F5 optional step: running programs linked the board's way (2.0.4 libc)

- User: "continue with F5 optional step". Analysis only, in scratch; nothing
  changed in `../minix-st-2.0.4` (read only, `rosco-m68k` at `c17cc1a`).
- Of the two options under F5 (bring ACK's own minix68k headers/libsys to
  2.0.4, or let the emulator run programs linked against the 2.0.4 C library)
  the second is the one that tests what the board runs; the first would
  duplicate a library the board does not use.
- Scratch: HEAD built (minix68k, all tests pass); the minix project's
  `src/rosco/ack/lib/Makefile` copied to scratch and run with this fork's
  ACK (`ACC`, `AAL`, paths overridden): `crtso.o`, `emrts.o`, `libc.a` build
  without errors. `lib/math` built as a `libm.a` the same way.
- **The emulator needs no change for the basics.** 2.0.4's `__sendrec`
  traps the same way (`trap #0`, d0 = 3, d1 = who, a0 = message), and
  messages, call numbers and errno values are the same. A test program
  (stdio, malloc, fopen "w"/"r", unlink, errno/strerror, float printf,
  atof, strtol, exit 3) linked
  `crtso.o emrts.o prog.o fphook.o libc.a libm.a libem.a libend.a`
  gives the right output and status 3.
- `tests/plat` `core`, `bugs`, `floats` (C files) linked that way (a
  `main()` that calls `_m_a_i_n` for the `,e,` tests): 35 of 37 pass.
  - `setjmp,c,`: 2.0.4's `setjmp` saves the signal mask with `SIGPROCMASK`
    (74), which the emulator does not know (2.0.4-only call).
  - `calloc,c,`: 2.0.4's `calloc` (`lib/ansi/calloc.c`) does not check
    `nelem * elsize` for overflow: `calloc(32767, 3)` wraps to 32765 bytes
    and succeeds. A 2.0.4 libc bug, for the minix project.
- Other findings for the minix project (not this repo):
  - **Float printf in ACK-built programs.** The ACK-built `libc.a` contains
    `fphook/fltpr.o`, the stub that prints "cannot print floating point".
    The real `_f_print` is `lib/fphook/fphook.c` (Minix's `libd.a`), pulled
    in when the compiler references `_fp_hook`, which this ACK (upstream
    too) never does. So `acc-minix` programs cannot print floats unless
    `fphook.o` (and `frexp`, `ldexp`, `__huge_val` from `lib/math`) is
    linked explicitly. Their gcc-built tools are fine (fixed in their
    Follow-up 79).
  - **`size_t` is 16 bits in 2.0.4** (`unsigned int`, `sys/types.h`),
    32 bits in ACK's own headers. `ack` adds its own include directory
    after `-I`, so a header 2.0.4 lacks (e.g. `<stdint.h>`) silently comes
    from ACK and redeclares `size_t` (seen with `tests/plat/lib/test.h`).
    Mixing the two header sets gives wrong calls (`memcpy`, `malloc`, ...
    take 2 bytes, not 4).
- Proposal (waiting for the user's choice):
  1. Emulator (`plat/minix68k/emu/sim.c`): add the 2.0.4-only calls the
     library uses, as no-ops where no signals are delivered: `SIGACTION`
     (71, writes `oact` = SIG_DFL), `SIGPROCMASK` (74, keeps the mask,
     returns it in `m2_l1`); others only when a test needs them.
  2. A test target that builds the 2.0.4 libc from a minix-st-2.0.4 tree
     with this ACK and runs `tests/plat` (+ a small libc test) linked the
     board's way. The sources are in another repository
     (`maarten-boot/minix-st-2.0.4`, public): locally from
     `MINIX204=../minix-st-2.0.4` (skipped when absent), and in CI a
     checkout of that repository at a pinned commit.
  3. Not do: port ACK's own minix68k headers/libsys to 2.0.4.
- User: "yes apply 1 and 2, with CI pinned".
  - Emulator (`plat/minix68k/emu/sim.c`): `SIGACTION` (71; per-signal
    `struct sigaction` kept and handed back, as MM), `SIGPROCMASK` (74; how
    in `m2_i1`, set and old mask in `m2_l1`, SIGKILL never blocked) and,
    found when running the tests, `SIGRETURN` (75): 2.0.4's `longjmp` goes
    through `_sigjmp` → `sigreturn(&sigcontext)`. Only the `SC_NOREGLOCALS`
    form (ACK's jmp_buf) is emulated, as the kernel does it: d0, a6, sp
    and pc; pc goes into the 68010 exception frame because the trap
    returns with `rte`, and d0 is not reset to 0 after the reply. A full
    register set (return from a signal handler) stops the run: no signal
    is ever delivered.
  - `plat/minix68k/tests/minix204.sh` and `make minix204-test`
    (`MINIX204=<tree>`, default `../minix-st-2.0.4`, skipped when absent):
    builds the library with the tree's own Makefile in a mirror of the
    tree's layout under `.obj/minix204` (symlinks; nothing written into the
    tree), plus `fphook.o` and `lib/math` as `libm.a`; runs `tests/plat`
    core/bugs/floats C tests (`minix204/emain.c` calls `_m_a_i_n` for the
    `,e,` tests; `minix204/stdint.h` so that ACK's, with a 32-bit
    `size_t`, is not used) and `minix204/libc.c`, whose output must equal
    `libc.out` (the same program's output with host gcc/glibc; identical).
    `core/calloc` is an expected failure (XPASS fails, so a fixed calloc
    is noticed when the pin moves).
  - CI: `build-linux` fetches `maarten-boot/minix-st-2.0.4` at
    `c17cc1a101cc015db99a44f94712c0379a809d2b` into `$RUNNER_TEMP` after
    `make`, then `make minix204-test`.
  - Scratch: full `make` plus `minix204-test` with the local tree and with
    a clean fetch of the pinned commit from GitHub: 37 passed, 1 XFAIL,
    rc 0; without a tree: skipped, rc 0. Before `SIGRETURN`: `setjmp` and
    `libc` failed ("unsupported system call 75"). Files identical to the
    tested scratch tree. Not committed.
- User: "pushed, check CI". Run 41239455715 (`c69c319de`): build-linux,
  build-linux-sanitizers, build-linux-clang all success. In build-linux the
  pinned minix-st-2.0.4 was fetched and `minix204-test` ran: library built,
  `core/calloc` XFAIL, "minix204: 37 passed, 0 failed".
- User: "yes tick F5; skip A3 but make a note". `Tasks.md`: F5 done
  (2026-10-31); A3 skipped (upstream only), with the fix noted: align
  `getarea()`'s blocks in `util/ass/asscm.c` (rounded to `sizeof(int)`
  today) to `_Alignof(max_align_t)`, as A1 did for led, and check with
  `-fsanitize=alignment` and PLATS = all.
- User: "do the crosscheck of Spec, Plan and code". There is no Spec.md or
  Plan.md in this repo: the plan, decisions and tests live in `Tasks.md`
  (with the logs), so `Tasks.md` was checked against the code.
  - All 74 paths named in `Tasks.md` exist, or are shorthand, install or
    release paths, or are in the minix-st-2.0.4 repository. Every `*_test`
    named is a build target. Each ticked item's key change was grepped in
    the code (E1, E3, E5, E6, F1, F1d, F3, F4, F7, F9, F11, N1, N3, N11,
    N12, L1, B2, S1, S3, A1, A2, P1, M1, F5, T4-T8, release workflow):
    all present. `minix204.sh`'s glob of tests/plat core/bugs/floats gives
    the same files as `tests/plat/build.py`'s list.
  - Mismatches, all in `Tasks.md` text (none in the code):
    1. Header: "Nothing here has been fixed yet" (most items are fixed);
       "Scope: C + BASIC" (BASIC deferred since 2026-10-30).
    2. "Tests to add" intro: "minix68k has no emulator and is not in
       `TEST_PLATS` ... nothing is tested", "BASIC ... not wired into the
       build": out of date since T1-T3 (minix68k is in `TEST_PLATS`).
    3. F8 ticked, but `plat/minix68k/libsys/build.py` still globs the
       nonexistent `plat/minix/libsys` (harmless dead entries; noted as
       remaining, never fixed or given an item).
    4. N8 says "Now `strlen()`"; B2 (`8a2103d9b`) later removed the DATA
       file name altogether, so that line no longer exists.
- User: "yes fix 1, 2 and 4, remove the dead glob". `Tasks.md`: header
  (what ticked/open mean; scope C and assembler, BASIC deferred), "Tests to
  add" intro (the 2026-10-29 state, and what runs now), N8 (superseded by
  B2), F8 (glob removed). `plat/minix68k/libsys/build.py`: the three
  `plat/minix/libsys` globs removed; scratch build passes (all tests) and
  `libsys.a` is byte-identical (128 members). Not committed.
