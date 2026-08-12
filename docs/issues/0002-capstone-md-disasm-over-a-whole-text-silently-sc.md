---
id: 2
title: capstone md.disasm() over a whole .text silently scans only a PREFIX and produced a confident FALSE NEGATIVE on this port's central question
status: resolved
symptom: a lui/addiu fold reports a tiny number of pairs (35 of 148,992 words) and the tool concludes 'nothing above .text is ever materialised'
tags: instrument,capstone,dead-end,mips
created: 2026-08-12
updated: 2026-08-12
---

**An instrument lied, mid-survey, on the single most important question in this tree**, and the shape of
the lie is worth carrying forward because the same idiom is one line away in any MIPS tool.

## What happened
The first version of the disc survey's loader probe used capstone's `md.disasm()` over the whole `.text`
to fold `lui`/`addiu` pairs and find materialised addresses. **`md.disasm()` STOPS DEAD at the first
undecodable word.** It therefore scanned only a prefix, folded **35 pairs out of 148,992 words**, and
printed a confident *"NEGATIVE: nothing above .text is ever materialised -> no fixed-base load target
exists"* — a false negative on "does this game load code overlays", which it does (21 of them).

The same defect made `BITS/MEMORY.BIN` read as data ("capstone decoded 17/15828 words") when per-word
decoding shows **95.0% code-plausible with 59 function prologues**. It is the largest code overlay on the
disc.

## Root cause
A diagnostic with **no denominator**. "35 pairs" and "17 words decoded" are indistinguishable from "the
tool worked and the answer is small" unless the tool prints how many words it actually examined.

## The fix, and the standing rule
Decode **every word independently** and REPORT the non-decoding ones as part of the denominator: the
fixed version folds **12,670 pairs** over all 148,992 words and prints **4,483 non-decoding words**.

**Any future use of capstone over a PSX `.text` in this workspace must decode per word.** Stated as a
directive in `CLAUDE.md` because the broken idiom is the shorter one to write.

## What it does NOT explain
The corrected fold still finds no `lui`/`addiu` pair materialising `0x800D1000` (3,501 refs to 761
distinct addresses above t_end, none of them the slot). That is a real finding, not this bug — see
`docs/re-frontier.md` RE-03.
