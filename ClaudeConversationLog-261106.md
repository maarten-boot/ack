# Conversation log 2026-11-06

- User: "status?". davidgiven/ack #391 (A3, ass-align) and #392 (N9,
  descr-lflag) were merged on 2026-11-03. Maintainer on #391: "Merging, thanks
  (although I think I'll just rip out the custom allocator here; they're
  usually a terrible idea)"; on #392: "Goes to show how little people actually
  use libraries on this. Merging, thanks." Also merged upstream: #393 (LLgen
  formatter, all .g files reformatted) and #394 (Update ab). Upstream default
  is now f735f0941, 19 commits past c9bccd0f0.
- User: "yes do 1 and 2, then sync default".
  - Tasks.md: A3 and N9 ticked, with the PR numbers.
  - Branches upstream/ass-align and upstream/descr-lflag: both commits are in
    upstream default (merge-base --is-ancestor), deleted locally. Deleting
    them on the fork failed (gh API: HTTP 403, token): the user deletes them.
  - Fork default (e445491b6, merge of fork PR #5) is upstream plus 170
    commits (the minix68k series), so no fast-forward: upstream is merged into
    rosco-minix68k as in 84c72ab4d, then a fork PR to default.
- Merge of upstream default f735f0941 into rosco-minix68k: 65 files (LLgen
  formatter and the reformatted .g files, #393; ab update, #394; util/ass
  from #391; descr -l lines from #392). One conflict, plat/minix68k/descr:
  the fork's `-fp SOFTFP=yes` against upstream's old Minix-ST `-i`/`-fp`/
  `-ansi` lines; kept the fork's side. Upstream's only change to that file
  since c9bccd0f0 was the #392 -l lines, which the fork already had.
  - make: rc 0, 2m51s, 116 TEST steps (incl. basic_divzero, diff_test).
  - schema 24 passed (libconf 2.0.1 from pip, as in CI), reader 120 passed,
    minix204 37 passed, 0 failed, XFAIL core/calloc (Minix-ST at 782d7b787).
  - Then: user pushes, CI, fork PR rosco-minix68k -> default.
