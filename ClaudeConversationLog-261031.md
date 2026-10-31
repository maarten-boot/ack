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
