# Claude conversation log — 2026-10-24

Continues `ClaudeConversationLog-261023.md` (findings F1–F10, proposed workflow in its section 4).
Open work is in `Tasks.md`.

## Session 2

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

### First CI run on the fork (run 40081763741, commit b59a37d7f)

Actions had to be enabled on the fork first (GitHub disables workflows on forks).
Result: **build-linux success**, build-macos failure (`make`), build-windows failure (`build`).
Upstream `davidgiven/ack` fails the same two jobs on our base commit `7afa32a0a`
(run 29642008449, still on the old action versions), so these failures predate the
action update. The update removed the "Node.js 20 is deprecated" warnings.
Job logs need admin rights, so they could not be read anonymously.

#### F11 — macOS failure: strict-aliasing UB in the Modula-2 front end (reproduced locally with clang)

- Reproduced with `make CC=clang HOSTCC=clang +all` on Linux (clang 21): fails at about
  the same log line as CI (≈1830 vs 1855):
  `lang/m2/libm2/PascalIO.mod`: `"ack.ack-....s", line 850: too big`.
- Cause traced through the passes: the clang-built `em_m2` emits the constant set
  `spaces = charset{11C..15C, ' '}` correctly on its first use, and garbage on later
  uses (`loc 15872` becomes e.g. `loc -1245336211`, different on each run). ncg then emits
  `move.w #<garbage>,-(sp)`, which the assembler rejects.
- Root cause: `lang/m2/comp/chk_expr.h` keeps the set reference count as an `int` inside
  the `arith` slot before the set:
  `#define inc_refcount(s) (*((int *)(s) - 1) += 1)` (likewise `dec_refcount`, `refcount`).
  `MkSet()` zeroes that slot through an `arith*` and then increments it through an `int*`,
  which breaks C's strict-aliasing rules; clang `-Os` reorders the two, so the count
  starts at 0, `FreeSet()` frees the constant after its first use, and later uses read
  freed memory.
  Verified: clang `-Os` build is wrong; the same build with `-fno-strict-aliasing` emits
  the right constants; an ASan `-O1` build is also correct (UB, not a plain memory bug).
  gcc happens to keep the order, so Linux CI passes, but the code is undefined either way.
- Suggested fix (one header, no behaviour change): keep the count in the `arith` slot
  with the same type:

  ```c
  #define inc_refcount(s)	(*((s) - 1) += 1)
  #define dec_refcount(s)	(*((s) - 1) -= 1)
  #define refcount(s)	(*((s) - 1))
  ```

  (all callers pass an `arith *`). Alternative quick fix: add `-fno-strict-aliasing` to
  `CFLAGS` in the Makefile.
- This only breaks Modula-2 code, which minix68k will no longer build (C + BASIC
  decision), but the other front ends could have similar casts; adding
  `-fno-strict-aliasing` globally is a cheap safety net for this old code base.

#### Windows failure: not yet diagnosed

- Fails very early in the `build` step (log line 91; upstream at line 192). Needs the job
  log (`gh run view 40081763741 --log-failed` with a logged-in `gh`, or a copy from the
  web UI).

### Decision: Linux only

ACK is only used on Linux. macOS and Windows builds are not of interest, so the macOS
(F11) and Windows CI failures are not pursued. F11 stays documented because it is real
undefined behaviour that a future gcc could also trip over.

Follow-up (applied 2026-10-24):
- `ccpp.yml`: remove the `build-macos` and `build-windows` jobs, keep `build-linux`.
- `release.yml`: deleted. It only built the Windows installer (`ack-setup.exe`) and only ran on
  `default`; the Linux minix68k release job from section 4 of `ClaudeConversationLog-261023.md` can replace it later.
- Fix the concurrency group (`github.head_ref || github.ref`) while editing `ccpp.yml`.
