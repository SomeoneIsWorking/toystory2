---
id: 11
title: Toy Story 2 boot reaches graphics init but never presents
status: resolved
symptom: Headless boot watchdog stays in 0x8003FA68 waiting on gp+0x7FC; no frame is presented
tags: render,vblank,host-turn,boot
created: 2026-08-22
updated: 2026-08-22
---

The guest registered VBlank callback 0x80039D60, but the game-local runtime seam never armed host field turns. The callback therefore never incremented gp+0x7FC or serviced deferred display work, so wait function 0x8003FA68 could not complete.

### Resolution (2026-08-22)
Added the game-local field clock seam: after exact guest graphics init 0x8003A650 it arms the retail 59.940 Hz host turn; each field samples pad, invokes registered guest VBlank 0x80039D60, advances SPU, and presents. Live headless boot changed from zero presents/watchdog to non-black legal and ESRB frames at presents 30/120/900, then progressed into FMV code.
