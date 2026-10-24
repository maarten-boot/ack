# Claude conversation log — ACK C compiler for 68k (68010) / rosco_m68k Minix

## Session 1 — 2026-10-23

### Request

> Inspect the current directory, keep track of findings in ClaudeConversationLog.md.
> The goal is to provide a C compiler for the 68k (68010) environment. Report obvious
> errors in the log and suggest fixes. Afterwards we need a GitHub Action to build
> only the C compiler for 68k so that it can be used by minix2 and bbcbasic for 68k.
> Initially make no changes other than in this log file.

Nothing in the repository was changed except this file. All test builds were done in a
throw-away `git clone` in the Claude scratchpad (`/tmp/claude-1000/...`).

---

### 1. What this repository is

- The **Amsterdam Compiler Kit (ACK)**, a fork of `davidgiven/ack` (upstream default branch
  `default`).
- Current branch `rosco-minix68k`, 4 local commits on top of upstream `7afa32a0a`:

  | commit | what |
  |---|---|
  | `eaa025860` | m68020 ncg: `con_mult()` emits `.data4` for 4-byte constants (fix for WORD_SIZE=2) |
  | `8625a2877` | minix68k cv: write a 32-byte header on 64-bit hosts |
  | `19b989ceb` | `sys_maketempfile()` for Minix (no `clock_gettime`) |
  | `dae8173bc` | Makefile: `PLATS = minix68k`, `DEFAULT_PLATFORM ?= minix68k` (local only) |

- Build system: `make` → `build/ab.mk` → Python (`build.py` files, "ab") → `ninja`.
  Needs GNU make ≥ 4, python3, lua, bison, flex, ninja, gcc.
- The relevant 68k pieces:

  | path | role |
  |---|---|
  | `plat/minix68k/` | target platform: `descr` (driver config), `boot.s` (crt0), `libsys/` (Minix syscalls via `trap #0`), `cv/` (ACK a.out → Minix-ST a.out converter), `include/` |
  | `plat/minix/include/` | shared Minix headers (`callnr.h`, `com.h`, `sys/stat.h`, ...) |
  | `mach/m68020/ncg` | code generator, built for minix68k with `-DWORD_SIZE=2 -DTBL68000=1` (68000 instruction set, 16-bit `int`, 32-bit `long`/pointer) |
  | `mach/m68k2/as` | 68000 assembler (also accepts 68010 `movec` etc.) |
  | `mach/m68k2/libem`, `libend` | EM runtime helpers |
  | `lang/cem/cemcom.ansi`, `cpp.ansi`, `lang/cem/libcc.ansi` | ANSI C front end, preprocessor, C library |

- Data model of `minix68k`: `int`=2, `short`=2, `long`=4, pointer=4, `float`=4, `double`=8,
  alignment 2 (Minix-ST / m68k2 ABI).

### 2. Build test (host: Linux x86_64, gcc, GNU make 4.4.1, lua 5.4)

- `make -j4 +compiler` in a fresh clone: **succeeds** in ~1.5 min. Staging tree
  (`.obj/staging`) is 6.2 MB: `bin/ack`, `lib/ack/{em_cemcom.ansi,cpp.ansi,em_led,em_opt,em_ego,...}`,
  `lib/ack/minix68k/{as,ncg,cv}`, `share/ack/minix68k/{boot.o,c-ansi.o,libc.a,libem.a,libsys.a,...}`.
- Only benign host warnings (unused `fscanf`/`write` results, etc.).
- `ack -mminix68k [-O|-O2|-O4|-O6] -o hello hello.c` with stdio, malloc, long arithmetic,
  struct initializers, switch, floats: compiles and links at all optimisation levels; the
  output has the expected Minix-ST header (`04 10 03 01 / 00 00 00 20 / text / data / bss ...`).
- `examples/{paranoia,startrek,mandelbrot}.c` compile and link too.
- Generated assembly of those programs contains **no 68020-only instructions**
  (checked for `extb`, `muls.l`, `divs.l`, bitfield ops, `rtd`, scaled index, `link.l`,
  `bra.l` ...), and libem/libsys/boot contain no `move sr,<ea>` (privileged on the 68010).
  So the *integer* code is 68000/68010-clean.
- The installed binaries can be relocated with `ACKDIR=<staging dir>` (checked in
  `util/ack/main.c`), so a tarball of `.obj/staging` is usable from anywhere.

Not tested: running anything on real hardware or an emulator (no 68k emulator here).

---

### 3. Findings (most severe first)

#### F1 — CRITICAL: floating point needs a 68881 FPU, which a 68010 system does not have

- `mach/m68k2/libem/build.py` globs **all** `*.s` in `mach/m68k2/libem`, including
  `fp68881.s`. That file implements every EM float helper (`.adf4/8`, `.mlf4/8`, `.dvf8`,
  `.cif8`, `.cff8`, `.cmf8`, ...) with `fmove`/`fadd`/... 68881 instructions.
- ncg (no `TBL68881`) emits `jsr .cff8`, `jsr .mlf8`, ... for any float/double code, and
  libc uses doubles internally (`printf("%g")`, `strtod`, `atof`, math library).
- Result: any program that touches floating point takes an **F-line trap** on a 68010
  without FPU. BBC BASIC uses floats everywhere, so it is directly affected.
- Root cause: ACK has a portable software-float library in `mach/proto/fp/*.c`, but its
  build recipe (`mach/proto/fp/build.lua`, only wired up for `cpm`) was deleted with the
  Lua build system in `414bbe1e6 "Finally delete ackbuilder!"` and never ported to the
  Python `build.py` system. `mach/m68k2/libfp/byte_order.h` (big-endian) is still there.
- Suggested fix:
  1. Port `mach/proto/fp/build.lua` (see `git show 414bbe1e6^:mach/proto/fp/build.lua`)
     to `mach/proto/fp/build.py`: compile each `.c` to `.s` with `ack -m<plat>`, run
     `massage_s.lua` on it, assemble into `libfp.a`, using
     `mach/m68k2/libfp/byte_order.h` for minix68k.
  2. Exclude `fp68881.s` from the minix68k libem (e.g. filter it out of the glob in
     `mach/m68k2/libem/build.py`, or build a separate `libem_fpu` for an `-fpu` flag).
  3. In `plat/minix68k/descr` link the soft-float library unconditionally before
     `libem.a` (`{PLATFORMDIR}/libfp.a`), and install it from `plat/minix68k/build.py`.
  4. Test with `examples/paranoia.c` (IEEE test program) in an emulator.

#### F2 — HIGH: heap buffer overflow in `cv` relocation emitter (`plat/minix68k/cv/cv.c`, `emit_relo()`)

- The relocation buffer is `malloc(4 * cnt)`, but the encoding needs 4 bytes for the first
  relocation, 1 byte per further relocation, **plus one extra `1` byte for every 254 bytes
  of gap** between consecutive relocations, plus the terminating 0. Sparse relocations
  (large initialised arrays between pointers) overflow the buffer.
- In addition `*((long*)b) = curr;` stores 8 bytes on LP64 hosts (only 4 are meant), and
  `cvlong()` byte-swaps through a `long*` assuming `sizeof(long)==4`.
- Reproduced: a program with `char *a="x"; char big[30000]={1}; char *b="y"; char big2[30000]={1}; char *c="z";`
  makes the shipped `cv` abort with `malloc(): corrupted top size`; built with
  `-fsanitize=address` it reports `heap-buffer-overflow ... cv.c:310`.
  Normal programs (dense code relocations) happen to fit, which is why it went unnoticed.
- Suggested fix: write the relocation table straight to the output with a small helper
  instead of a pre-sized buffer, e.g.

  ```c
  static void put1(int c) { unsigned char b = c; if (write(output, &b, 1) != 1) fatal("write error\n"); }
  static void put4(uint32_t v) { put1(v >> 24); put1(v >> 16); put1(v >> 8); put1(v); }
  ...
  if (last == 0) { last = curr; put4(curr); }
  else { while (curr - last > 254) { put1(1); last += 254; } put1(curr - last); last = curr; }
  ...
  if (last) put1(0); else put4(0);
  ```

  (Also note the existing loop uses `> 255` but then emits `curr - last` which may be
  255 — an odd value, which the format comment calls *illegal*. Relocations are always
  even here, so with `> 254` the emitted byte is always even and ≤ 254.)
  Drop `cvlong()`; check the `write()` return values (current gcc warnings at cv.c:323/326).

#### F3 — MEDIUM: default stack size from `cv` can be tiny

- `minixhead()` sets `a_total` to the next multiple of 64 KB above text+data+bss, so the
  stack is whatever is left in that 64 KB block: 48 KB for hello world, but only
  4.5 KB in the F2 test program, and it can be a few bytes for a program that ends just
  under a 64 KB boundary. Large programs like BBC BASIC (and the ACK passes themselves
  if they are ever run on Minix) can crash with a stack overflow. You can override it
  with `cv +N`/`=N` (chmem syntax) but `descr` never passes that flag.
- Suggested fix: enforce a minimum stack (e.g. `if (stack < 16384) stack += 0x10000;`), or
  pass a sensible default from `descr` (`args {CVSTACK?} < >`, with a `mapflag -stack*`),
  and document `chmem` for users.

#### F4 — MEDIUM: `plat/minix68k/descr` has leftovers from the old Minix-ST setup

- `var NAME/M/MLIB/RT/LIB`, `SYSINCLUDES=-I{EM}/lib/minixST/include ...`,
  `C_LIB={EM}/{LIB}cc.1s {EM}/{LIB}cc.2g`, `OLD_C_LIB` point to files that no longer exist
  (`lib/minixST/...`). They are mostly unused, but:
  - `mapflag -ansi C_LIB={EM}/{LIB}ac` and `mapflag -l* LNAME={EM}/{LIB}*` point to
    non-existent paths, so `ack -lm`/`-l<x>` cannot work. linux68k uses
    `LNAME={PLATFORMDIR}/lib*`.
  - `mapflag -fp LIBFP={EM}/{NLIB}fp` sets a variable that is never used (`FLOATS` is what
    `args` references) and `NLIB` is undefined — `-fp` is a no-op. (Will be replaced by F1.)
  - `MACHOPT_F=-ml10` is a leftover option for the old m68k2 cg.
- `CPP_F=-D__unix -D_ATARI_ST`: does not define `__minix`, `__m68k`, `__mc68000`
  (or `__mc68010`). Consequences: code (including this branch's own
  `modules/src/system/maketempfile.c` change, which tests `#if defined __minix`) does not
  see it is building for Minix when compiled by this ACK. Suggested:
  `var CPP_F=-D__unix -D__minix -D__m68k -D__mc68000 -D__mc68010 -D_ATARI_ST`
  (keep `_ATARI_ST` only if the Minix headers in use need it; rosco is not an ST).
- No `EGO_PLAT_FLAGS`: the global optimizer (`-O2` and up) runs without the machine
  description. It works (tested) but optimises less well. `util/ego/descr/m68k2.descr`
  exists but is not installed; add it to the ego descr installation and set
  `var EGO_PLAT_FLAGS=-M{EM}/share/ack/ego/m68k2.descr`.
- Suggested fix: rewrite `descr` on the model of `plat/linux68k/descr`, keeping the
  minix68k type sizes, `-c` for led and the `cv` step.

#### F5 — MEDIUM: verify the a.out header and ABI against the rosco Minix 2 kernel

These are not provably wrong, but must match whatever the rosco_m68k Minix 2.0.4 kernel
expects; please check against its `include/a.out.h` and `mm/exec.c`:
- `cv` writes the **Minix-ST 1.x** header layout: bytes `04 10 03 01` (a_cpu=0x04 — which in
  standard Minix 2 headers is `A_I8086`, the 68k id is `A_M68K = 0x0B` — then
  a_flags=0x10 `A_EXEC`, magic `03 01`) and `00 00 00 20` (hdrlen 0x20 in the 4th byte).
  Standard Minix 2 uses `a_magic[2]={0x01,0x03}, a_flags, a_cpu, a_hdrlen, a_unused, a_version`.
  `plat/minix68k/include/a.out.h` describes the ST layout, so it is consistent with itself;
  commit `8625a2877` says binaries now run, which suggests the kernel uses the ST layout.
- Relocation table in GEMDOS format appended after data: required for a no-MMU Minix-ST
  style loader; again must match the rosco kernel.
- **16-bit `int`**: the syscall glue and message structures (`_M.m1_i1` etc.) assume the
  kernel was compiled with the same 16-bit-int ACK (Minix-ST convention). If the rosco
  Minix 2 kernel was compiled with gcc / 32-bit int, every syscall message layout
  (and `struct stat` with `short st_nlink`) would be wrong. Confirm before building bbcbasic.
- `plat/minix/include/minix/callnr.h` is Minix 1.x (NCALLS 70, AM_SYSCALL 69); Minix 2.0.4
  adds e.g. `SETSID`, `GETPGRP`, `SIGACTION`, `SIGSUSPEND`, `SIGPENDING`, `SIGPROCMASK`,
  `SIGRETURN`, `REBOOT`, `SVRCTL`. Programs needing POSIX signals will need these.

#### F6 — LOW: `boot.s` (crt0) is copied from another port

- Header comment says *"EM runtime start-off for the Bleasdale 68000 system"*.
- `move.l 4(sp),a0 ; clr.l -4(a0)` clears the 4 bytes **before the first argument string**,
  i.e. it relies on that being the envp NULL terminator in the Minix-ST stack layout.
  Works if the kernel builds the stack exactly like Minix-ST; worth a comment and a test with
  empty environment / many arguments on rosco.
- `add #010,sp` after `jsr __m_a_i_n` does not match the 10 bytes pushed (harmless because
  `EXIT` follows, but confusing).

#### F7 — LOW: `modules/src/system/maketempfile.c` (commit `19b989ceb`)

- The `__minix` branch is only active when the ACK passes are compiled for Minix by a
  compiler that defines `__minix` (not this ACK's minix68k, see F4).
- With 16-bit `unsigned` (ACK minix68k) the borrowed `temper()` constants
  (`0x9D2C5680`, `0xEFC60000`, `x<<15`, `x>>18`) truncate/overflow: `x>>18` on a
  16-bit value is undefined. Randomness is poor but the `O_EXCL` retry loop keeps it
  correct. Suggest using `unsigned long` in `temper()`/`rand_r()` under `__minix`.
- `(unsigned int)(long)prefix` — fine, but `intptr_t` is not available in Minix 2 so this
  is the right call.

#### F8 — LOW: smaller code issues in `plat/minix68k`

- `cv.c`: `fatal("... not %ld\n", NSECT, outhead.oh_nsect)` — `oh_nsect` is `uint16_t`,
  printed with `%ld` (undefined behaviour, garbage on LP64). Use `%d`.
- `cv.c`: `emits()` mallocs/frees a 16 KB buffer per 16 KB chunk; allocate once.
- `cv.c`: `chmem()` leaves `new` uninitialised for unexpected prefixes (unreachable today).
- `plat/minix68k/libsys/build.py` globs `plat/minix/libsys/*` which does not exist
  (harmless, but misleading).
- `plat/minix68k/libsys/LIST`, `mach_params` are leftovers from the pre-ab build.

#### F9 — LOW: build-system / CI observations

- `build/_sandbox.py`: `join(sandbox, f)` discards the sandbox when `f` is absolute, and
  relative paths with `..` break too, so `make OBJ=<dir outside the tree>` fails
  (`FileNotFoundError` in `os.rename`). Use the default `.obj`, or `AB_SANDBOX=no`.
- `.github/workflows/ccpp.yml` runs Linux + macOS + Windows on every push of every branch;
  `concurrency.group` uses `github.head_ref`, which is empty on pushes, so pushes to
  different branches cancel each other. `actions/checkout@v3` is deprecated (Node 16).
- `.github/workflows/release.yml` runs on pushes to `default` and force-pushes a `dev` tag
  as user "davidgiven" and creates a "Nightly" release — on this fork, disable it or
  change it.
- `.drone.yml` / `.travis.yml` are dead (Ubuntu trusty, target `+ack` no longer exists).
- Makefile refuses `PLATS=` on the command line, so a CI job cannot select the platform;
  this branch hard-codes `PLATS = minix68k` (fine for this fork). For upstreaming, make
  `PLATS ?=` overridable.
- `+compiler` builds all front ends (C, Pascal, Modula-2, Basic) and their libraries for
  minix68k. "C only" would need a new export target (see section 4); the saving is small
  (whole build is ~1.5 min on 4 cores).
- There are no tests for minix68k (`TEST_PLATS` excludes it; no 68k Minix emulator in CI).

#### F10 — INFO: 68010 specifics

- The 68000 instruction set from `TBL68000` runs unchanged on a 68010; nothing in the
  user-mode runtime uses `move sr,<ea>` (privileged on 68010), so user programs are fine.
- For the **kernel** side (minix2), 68010 differs from 68000 in exception stack frames
  (format word), `move sr` privilege, VBR (`movec`). The assembler supports `movec`; the
  kernel source must handle those — not a compiler issue.
- No 68010-specific code generation (loop mode `dbra` tight loops) is done; that is an
  optimisation only.

---

### 4. Proposed GitHub Action (not yet added)

Proposed file: `.github/workflows/minix68k.yml`. It builds the host cross-compiler with
`PLATS = minix68k` (as set by this branch's Makefile), smoke-tests it, and publishes a
relocatable tarball that the minix2 and bbcbasic repos can download and use with
`ACKDIR=<unpacked dir> <unpacked dir>/bin/ack -mminix68k ...`.

```yaml
name: minix68k C compiler

on:
  push:
    branches: [rosco-minix68k]
    tags: ["minix68k-*"]
  pull_request:
  workflow_dispatch:

concurrency:
  group: minix68k-${{ github.ref }}
  cancel-in-progress: true

permissions:
  contents: write   # needed only for the release step

jobs:
  build:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4

      - name: Install build dependencies
        run: sudo apt-get update && sudo apt-get install -y bison flex lua5.4 ninja-build

      - name: Build
        run: make -j"$(nproc)" PREFIX=/opt/ack-minix68k +compiler

      - name: Smoke test
        run: |
          export ACKDIR="$PWD/.obj/staging"
          cat > /tmp/t.c <<'EOF'
          #include <stdio.h>
          int main(void) { long l = 70000L; printf("%ld %d\n", l, (int)sizeof(int)); return 0; }
          EOF
          for o in "" -O -O2; do "$ACKDIR/bin/ack" -mminix68k $o -o /tmp/t /tmp/t.c; done
          head -c 4 /tmp/t | od -An -tx1 | grep -q '04 10 03 01'
          for f in examples/paranoia.c examples/startrek.c; do
            "$ACKDIR/bin/ack" -mminix68k -O -o /tmp/x "$f"
          done

      - name: Package
        run: |
          tar -C .obj/staging -czf ack-minix68k-linux-x86_64.tar.gz .
          sha256sum ack-minix68k-linux-x86_64.tar.gz > ack-minix68k-linux-x86_64.tar.gz.sha256

      - uses: actions/upload-artifact@v4
        with:
          name: ack-minix68k-linux-x86_64
          path: ack-minix68k-linux-x86_64.tar.gz*

      - name: Release
        if: startsWith(github.ref, 'refs/tags/minix68k-')
        uses: softprops/action-gh-release@v2
        with:
          files: ack-minix68k-linux-x86_64.tar.gz*
```

Consumers (minix2, bbcbasic workflows) would then do:

```sh
curl -L -o ack.tgz https://github.com/maarten-boot/ack/releases/download/minix68k-<ver>/ack-minix68k-linux-x86_64.tar.gz
mkdir ack && tar -C ack -xzf ack.tgz
export ACKDIR=$PWD/ack PATH=$PWD/ack/bin:$PATH
ack -mminix68k -O -o prog prog.c
```

Decisions still open for this step:
- **"Only the C compiler"**: either accept `+compiler` (includes Pascal/M2/Basic, ~1.5 min)
  or add a `minix68k_c` export to the top-level `build.py` with only
  `lang/cem/cemcom.ansi`, `lang/cem/cpp.ansi`, `util/ack`, `util/led`, `util/opt`,
  `util/ego`, `util/arch`, `util/amisc`, `plat/minix68k+tools`, libc/libem/libend/libsys and
  `boot.o` (needs a C-only variant of `build_plat_libs`).
- Keep, restrict or disable the existing `ccpp.yml` (Windows/macOS) and `release.yml` on
  this fork (F9).
- Whether to also build the ACK passes as Minix-native binaries (implied by commit
  `19b989ceb`) — a separate, larger job.
- Pin release naming/tagging scheme (`minix68k-YYYYMMDD` suggested).

### 5. Suggested order of work

1. F2 (cv overflow) — small, self-contained, reproducible.
2. F1 (soft-float library) — required for BBC BASIC on a 68010.
3. F4 (descr cleanup, `__minix`/`__m68k` defines, `-l` paths, ego descr).
4. F5 checks against the rosco Minix 2 kernel sources (header, 16-bit int, syscalls).
5. Add the workflow from section 4; then F3, F6–F9 as time permits.
6. Longer term: run `tests/plat` style tests for minix68k under a 68k emulator in CI.

---

## Session 1 (continued) — 2026-10-23

### Decisions from the user

- Scope of the compiler build: **C + BASIC** (ACK BASIC front end, `lang/basic`).
  Modula-2 and Pascal are **not** included for now.
- Floating point (F1) and the other ABI questions (F5: a.out header, 16-bit `int`,
  syscall numbers) will be decided after looking at the **minix2** and **bbcbasic**
  projects, which the user will link in later. No source changes until then.

### Plan for the C + BASIC build (proposed, not applied)

- `plat/build.py`: give `build_plat_libs()` a `langs` parameter (default all four, so
  the other platforms are unchanged) and build only the libraries for the listed languages.
- `plat/minix68k/build.py`: `build_plat_libs(..., langs=("c", "basic"))`.
- top-level `build.py`: new export `c_basic` = cemcom.ansi + cpp.ansi + basic front end
  + the shared tools (`util/ack`, `led`, `opt`, `ego`, `arch`, `amisc`, `misc`) + `plat/<p>+all`.
  The workflow in section 4 would then run `make +c_basic` instead of `+compiler`.
- `examples/build.py`: limit minix68k to `hilo.bas`, `hilo.c`, `mandelbrot.c`,
  `paranoia.c`, `startrek.c`, so the default `make` does not try to link `.mod`/`.p`
  programs without their libraries.
- Smoke test in the workflow: also compile `examples/hilo.bas`.

### Open items

- Link the minix2 and bbcbasic projects; then decide F1 (soft-float vs FPU), F5.
- Whether to keep/limit/disable the existing `ccpp.yml` and `release.yml` on this fork.

---

## Session 2 — 2026-10-24

### Decision: keep `ccpp.yml` and `release.yml`, update and pin all actions

`release.yml` only runs on pushes to `default`, so it is inactive on `rosco-minix68k`;
the CI (`ccpp.yml`) is useful to keep. All actions are now pinned to the commit SHA of
their latest release (looked up with `git ls-remote --tags`; all run on node24):

| action | was | now |
|---|---|---|
| `actions/checkout` | `@v3` | `v7.0.1` `3d3c42e5aac5ba805825da76410c181273ba90b1` |
| `msys2/setup-msys2` | `@v2` | `v2.33.0` `ec48f7c5447b3140e2b088413ae3a55687bccb6e` |
| `actions/upload-artifact` | `@v4` | `v7.0.2` `cf430e030ddbb5b0abf93d22962f4752f3646cd9` |
| `softprops/action-gh-release` | `@v1` | `v3.0.3` `e598afbe1493e6b1bafb1f389cabb956eab91231` |
| `mknejp/delete-release-assets` | `@v1` | **removed** |

- `mknejp/delete-release-assets` no longer has any tags (so `@v1` cannot resolve) and
  runs on deprecated node20. It is not needed: `action-gh-release` overwrites assets with
  the same name (`overwrite_files` defaults to `true`).
- `release.yml` got `permissions: contents: write`, needed for `git push -f origin dev`
  and the release update with today's read-only default `GITHUB_TOKEN`.
- Not changed (still open): `concurrency.group` uses `github.head_ref`, which is empty on
  pushes, so pushes to different branches cancel each other (`github.head_ref || github.ref`
  would fix it); `release.yml` still tags as "davidgiven".
