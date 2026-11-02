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
