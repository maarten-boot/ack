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
