---
id: 4
title: prior-art.md's TS2 entry missed a live MIT decomp of a sibling title, because the search was per-TITLE
status: resolved
symptom: a prior-art survey reports no decomp for a game, while an unlisted decomp of a sibling title on the same engine exists under a different name
tags: workflow,prior-art,search
created: 2026-08-12
updated: 2026-08-12
---

A **workflow defect**, not a bug in this game — recorded here because the next three trees the workspace
plans are affected by it.

## What was missed
`external/psxport/docs/prior-art.md`'s TS2 entry (written 2026-08-12) records that no decomp exists. True
for TS2 — but it also missed **`mateusfavarin/tsr`, MIT-licensed, a live decompilation of Toy Story Racer
(`SLUS_012.14`)**, i.e. a sibling Traveller's Tales PS1 title on the descendant of this engine, last
pushed 2026-06-11. It is not on decomp.dev and could never have been found by searching for "Toy Story
2": **the artifact is named after a different game.**

## The generalisable lesson
**For any new title, search the STUDIO and its sibling titles, not just the title.** A single-title search
cannot find an artifact named after a sibling.

## Why it matters beyond this tree
The same author (`mateusfavarin`) owns MIT-licensed RE work on **Crash Bash, CTR and Harry Potter PS1** —
which bears directly on decisions `WORKSPACE.md` has already recorded for the `crash`, `ctr` and
`crashbash` trees. **Check that before any of those trees is created.**

## Second omission in the same entry, fixed here
`prior-art.md` lists `juanmv94/TravellersTalesPSXCollisionViewer` and `mouksx/Toy-Story-2-Modding`
**without recording that both are unlicensed** (no LICENSE file, GitHub `license: null`), i.e. all rights
reserved, i.e. read-only. Anyone skimming that table could reasonably vendor one. `docs/references.md` in
this repo carries a licence column for every project; the upstream table still needs one, and only the
operator may edit the framework repo.
