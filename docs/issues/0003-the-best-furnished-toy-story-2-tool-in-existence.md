---
id: 3
title: the best-furnished Toy Story 2 tool in existence targets a container that is NOT on the PSX disc (.NGN)
status: dead-end
symptom: a TS2 tool or note describes .NGN files holding models, animations, geometry, textures and area portals, and no such file exists on the disc
tags: prior-art,dead-end,formats,cross-platform
created: 2026-08-12
updated: 2026-08-12
---

**MEASURED and falsified before any time was spent on it. Recorded so the next session does not spend a
day on the best-looking tool in the field.**

`EpicMinecartz/ToyTwoToolbox` (~11 MB, a full C# editor with Character / Animation / AreaPortal /
Geometry / Material / AShape classes and `.SAV`/`.NGN` load-save) is the most substantial TS2 tool that
exists, and it is built **entirely** around the `.NGN` container. `mouksx`'s README calls NGN "the first
of the 3 data files needed to load a level".

## The measurement
* `grep -ci NGN` over the **300 filesystem entries `tools/discdump.py list` returns → 0 of 300** (an
  earlier "303" was a `wc -l` of that listing including its trailer lines).
* The obvious blind spot closed too — an `NGN` magic INSIDE a container: grepped the extracted
  `LEVEL.RAW` (469,276 B), `LEVEL.DAT` (437,164 B) and `TERRAIN.ALL` (116,368 B) → **0 hits in all
  three**.
* Residual blind spot, stated: 3 of 46 `.RAW` files were grepped and no `.BIN`/`.ANM`/`.STR` were. So
  *"no NGN in the ISO9660 filesystem"* is fully closed; *"no NGN anywhere in any container"* is not.

## Conclusion
`.NGN` belongs to the **PC** (and/or N64) data build. On PSX that content is split across
`.ALL`/`.ANM`/`.DAT`/`.BIN`. Every NGN-based tool and note therefore describes a different platform's
build and is **not evidence about our binary** — including `Toy2LevelDump` (GPL-3.0, i.e. legally
vendorable and useless).

## The generalisation, which is the part worth keeping
TS2 shipped on PS1, N64, PC and Dreamcast and the community lives on the PC build. **Label every source
with its platform before believing it.** Only juanmv94 (PSX savestates) and mouksx's `.RAW`/`.DAT` half
are PSX-derived. See `docs/references.md`.
