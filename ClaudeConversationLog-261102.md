# Conversation log 2026-11-02

- CI 41357473992 (rosco-minix68k, 4ecce7b58, from yesterday): all three
  jobs succeeded; minix204-test 37 passed, 0 failed, XFAIL core/calloc.
- Status: all twelve upstream PRs (#376-#389) are merged; #376 at
  2026-11-02 00:22 ("Okay, cheers! ... Merging!"). Upstream default
  65f9981b2 CI green.
- Fork run 41363164891 on upstream/float-fold failed (build.py:60 "unmatched
  ')'"): c8caa951e is davidgiven's merge of default into the branch; he
  fixed it in 91de3e120 ("Typo fix.") before merging #381. Nothing to do.
- minix-st-2.0.4 still at c17cc1a; calloc not fixed.
- User: "ok try 3" (trial merge of upstream default into rosco-minix68k).
  - Upstream default c9bccd0f0 (61 commits); merge base 7afa32a0a; the
    fork has 156 commits of its own.
  - Scratch worktree, `git merge --no-commit`: 4 conflicts.
    - tests/plat/build.py: kept the fork's initlong, mulcc, tstword (and
      cmulong, zeroext, which merged cleanly) in the core set.
    - .gitignore: both sides' entries.
    - release.yml: stays deleted (fork is Linux only).
    - ccpp.yml: the fork's three Linux jobs plus upstream's new pieces:
      PYTHONPATH env, workflow-named concurrency group, apt libconfig-dev
      pkg-config python3-pip, pip libconf==2.0.1, and the two description
      test steps in build-linux.
    - Also minix68k-release.yml: the same dependencies (it runs make).
  - New upstream dependency: ego descr files use libconfig (9b5771bde);
    user installed libconfig-dev locally; libconf into .obj/python-deps.
  - Results: make rc 0, 114 tests (112 before); minix204-test 37 passed, 0
    failed, XFAIL core/calloc; schema tests 24 passed, reader tests 120
    passed; sanitizer build rc 0, 114 tests; clang build rc 0, 114 tests.
  - ego passes bo cj cs il lv ra sp sr ud now link libconfig dynamically:
    the release tarball needs libconfig on the target host for ego (-O).
  - User: "yes go ahead". Merge committed as 84c72ab4d; rosco-minix68k
    fast-forwarded to it. Scratch worktree removed.
- User pushed. CI 41484714837 (rosco-minix68k, f9579a49b, first run with
  the merge): build-linux, -sanitizers, -clang succeeded; the new apt and
  pip dependencies installed; schema tests 24 passed, reader tests 120
  passed; minix204-test 37 passed, 0 failed, XFAIL core/calloc.
- User opened and merged fork PR #4 "Rosco minix68k sync from upstream"
  (rosco-minix68k -> default) as 8be038582.
  - 41486842208 (PR #4) and 41486861923 (default, 8be038582): all three
    jobs succeeded in both; schema 24, reader 120, minix204 37 passed, 0
    failed.
  - minix68k-release.yml has the new dependencies but has not run yet (only
    on a minix68k-* tag or a manual run).
- CI 41488171119 (rosco-minix68k, b45ae9847): all three jobs succeeded;
  schema 24, reader 120, minix204 37 passed, 0 failed, XFAIL core/calloc.
- Branch cleanup. The fork had 15 upstream/* branches; 14 were exactly at
  the head of their merged davidgiven/ack PR (#375-#387, #389), and
  upstream/basic-repro (97209cf88, no PR) has the same patch-id as
  9dee5b498, which is in upstream default via #375.
  User deleted all 15 on GitHub and locally (first attempt did not take;
  rechecked: 0 left). The fork now has only default and rosco-minix68k.
  Heads, for restoring: basic-data-test 9f150bd9d, basic-fixes b83a4b45d,
  basic-repro 97209cf88, ci-actions d406b33a7, codegen-fixes 7eb551ec9,
  cpp-escape cb29db892, cpp-longpath 509093957, ego-fixes 138489ccd,
  float-fold c8caa951e, led-align 38656bd76, libc-fixes e6e11b09d,
  ncg-gc-cc 1e3151950, tests-plat-wordsize 1155d5a4f, ub-fixes 1c1a655da,
  volatile-nop c1042edd0.
- CI 41496226471 (rosco-minix68k, 5228bf54b): all three jobs succeeded;
  schema 24, reader 120, minix204 37 passed, 0 failed, XFAIL core/calloc.
- Manual run of minix68k-release.yml (user dispatched it; the gh token here
  gets HTTP 403 on workflow dispatch): 41510099767 on 5228bf54b succeeded.
  apt and pip steps, build and test, package, upload passed; release
  skipped (not a tag). Artifact ack-minix68k (3.5 MB):
  ack-minix68k-20261102-5228bf54b-linux-x86_64.tar.gz, smoke test ok
  (hello rosco 70000 2 1414.214).
  - Checked the tarball with ldd: lib/ack/ego/{bo,cj,cs,il,lv,ra,sp,sr,ud}
    need libconfig.so.9; bin/ack and the other passes do not. ego runs only
    for -O2 and higher ("optimizer 2" in lib/descr/fe), so -O and the smoke
    test (-O) work without libconfig. (Correction: the entry above said
    "ego (-O)"; it is -O2 and up.)
  - User: "yes add the libconfig note to the release docs".
    plat/minix68k/mkrelease.sh: README-minix68k.txt in the tarball says ego
    (-O2 and up) needs libconfig 1.7 (libconfig.so.9), package libconfig9 on
    Debian and Ubuntu.
- CI 41518635686 (rosco-minix68k, e6b42068d): all three jobs succeeded;
  schema 24, reader 120, minix204 37 passed, 0 failed, XFAIL core/calloc.
- User: "an -O2 compile in the release smoke test and list the parked
  tasks now".
  - plat/minix68k/mkrelease.sh: the smoke test compiles and runs hello.c
    with -O and with -O2 (-O2 runs ego, so a missing libconfig or a broken
    ego pass now fails the release); message "smoke test ok, -O and -O2".
  - Tried on the tarball of 41510099767 on this host: -O passes; -O2 fails
    with "lib/ack/ego/cj: error while loading shared libraries:
    libconfig.so.9", and ack exits non-zero, so the test catches it.
  - Found on the way: this host has libconfig 1.7.3 as libconfig11
    (libconfig.so.11); the CI runner (Ubuntu 24.04) builds against libconfig9
    1.5-0.4build2 (libconfig.so.9). So the soname, not the version, is what
    the tarball needs: README text corrected to "libconfig.so.9: the
    libconfig9 package on Debian 12 and Ubuntu 22.04/24.04. Newer releases
    ship libconfig.so.11 (libconfig11), which does not do."
    Not done (a choice for the user): link libconfig statically in the
    release build so the tarball runs on both.
  - The -O2 pass on CI is checked by the next manual release run.
  - Parked in Tasks.md (7 open): F1c, B1, P2, C + BASIC target, A3, N9, N6.
- User: "yes commit, yes link statically; look at Build minix68k with C and
  BASIC; is it even possible to fix BASIC ON ERROR can't catch arithmetic
  errors on minix68k?" Committed the -O2 smoke test as 02609eafb.
- Static libconfig for the release.
  - The build takes libconfig's flags from pkg-config (build/pkg.py,
    util/ego/build.py). minix68k-release.yml: new step "static libconfig"
    writes a copy of libconfig.pc with "Libs: -L${libdir} -l:libconfig.a"
    to $RUNNER_TEMP/pc-static and sets PKG_CONFIG_PATH. No upstream files
    changed.
  - mkrelease.sh: before packing, fails if any packaged program needs a
    shared library besides glibc (ldd); README now says the programs need
    only glibc, and the ego passes contain libconfig, linked statically
    (LGPL 2.1 or later, source link; rebuild ACK from the commit for
    another libconfig).
  - Tried in a scratch worktree of 02609eafb with the same .pc override:
    make rc 0 (1m46s, all minix68k tests), every staged program and the
    emulator need only libc.so.6/ld-linux; mkrelease (new): check passed,
    smoke test ok with -O and -O2 on this host, which has no libconfig.so.9.
    Negative check: the same ldd test on the 41510099767 tarball lists
    lib/ack/ego/{bo,cj,cs,...}: libconfig.so.9.
  - Not yet seen: libconfig.a in Ubuntu 24.04's libconfig-dev 1.5 (it is in
    1.7.3 here); the next manual release run shows it (link fails if not).
- C + BASIC target: already works. build_plat_libs() builds the libraries
  of C, BASIC, Modula-2 and Pascal for every plat; the tarball has em_bem
  and libbasic.a, examples build hilo.bas, the BASIC tests run in the
  emulator. The task only adds dropping Modula-2 and Pascal: em_m2 508K,
  em_pc 448K, libmodula2.a 135K, libpascal.a 44K (about 1.2 MB of 8.6 MB),
  at the cost of fork changes in plat/build.py, build.py and
  examples/build.py. Suggest closing it. Noted in Tasks.md.
- B1 (BASIC ON ERROR and arithmetic errors): possible, and without signals.
  - EM's `sig` installs a trap handler; m68k2 libem .trp calls it (.trppc)
    with the trap number instead of printing "trap N called" and exiting,
    and clears .trppc first. Pascal (lang/pc/libpc/encaps.e, sig.e) and
    Modula-2 use it. head_ac.e has such a handler (_ctch_: EIDIVZ/EFDIVZ ->
    kill(getpid(), 8)) under CATCHHANDLER, which is not defined; kill()
    would need signal delivery in the emulator.
  - Fix: a small .e file in lang/basic/lib: handler _trpem(trapno):
    EIDIVZ (6) and EFDIVZ (7) -> _trpfatal(8) (8 = SIGFPE, as test17 and
    test18 expect); other traps -> `lol 0; trp` (default message and exit).
    _ini_trp() in trap.c installs it with `sig`; _trap() already calls
    _ini_trp() again after a caught error, which reinstalls it. Generic for
    every platform where these errors are EM traps; on hardware-trap
    platforms SIGFPE still goes through signal().
  - Test: test17 (FATAL ERROR: trap 8) and test18 up to "division error
    catched 8 130" in the emulator; test18's SIGINT part cannot run
    unattended anywhere.
  - Noted in Tasks.md. Not implemented (BASIC deferred since 10-30; user's
    decision).
