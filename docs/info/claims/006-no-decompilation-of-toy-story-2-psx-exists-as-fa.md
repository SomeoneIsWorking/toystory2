---
id: C006
kind: claim
status: holds
created: 2026-08-12
tags: prior-art,supply
reconfirmed: 2026-08-24 20:19:56
verified_at: 2026-08-24 20:19:56
depends: docs/references.md
---

## Claim

No decompilation of Toy Story 2 (PSX) exists as far as a GitHub + decomp.dev search can establish — this port has NO symbol map, NO function boundaries and NO matching build, and this is a SEARCH NEGATIVE, which is weaker than a measurement

## Evidence

SEARCHED 2026-08-12, and the method is recorded so the next session does not repeat it: gh api on every named repo plus git/trees?recursive=1 and the README of each; gh search repos over toystory2, toy story 2, buzz lightyear, travellers tales, toy story racer, bugs life playstation, muppet race mania, weakest link psx, rascal psx, RNC ProPack, Rob Northen compression, EpicMinecartz, psx-modding-toolchain; the full 17-repo listing of users/mateusfavarin; a full read of decomp.dev?platform=ps1 (SotN, Rage Racer, KH 358/2, Dark Cloud, NFS High Stakes, Xenogears, Legend of Mana, Gauntlet DL, Vagrant Story, RE CVX, God Hand, FF7, SH1/2/3, Parasite Eve II, Fatal Frame, Digimon World, Tomba! — nothing for Toy Story 2 and nothing for ANY Traveller Tales title); and two web searches including the engine author name Dave Dootson. WHAT RAISES CONFIDENCE WITHOUT PROVING ANYTHING: the community centre of gravity is visibly elsewhere — every active TS2 project targets the PC build (RibShark/ToyStory2Fix 62 stars, ManiacKnight/ToyStory2FixPlusPlus, JeffRuLz widescreen tool, mouksx/t2gm2) and cheeseandcereal explicitly scopes itself to PC US, PC UK and Project64. Nobody is decompiling the PSX executable. WHAT WAS FOUND INSTEAD, and it is a real find that is NOT a decomp of this game: mateusfavarin/tsr, MIT, a decompilation of Toy Story Racer (SLUS_012.14), 14 of 339 main-executable functions, last pushed 2026-06-11 — a DIFFERENT game on the descendant of this engine. Its addresses belong to SLUS_012.14 with NO translation, so its symbols are worthless as addresses and valuable only as names, structures and mechanisms to re-find here with Ghidra. THE CONSEQUENCE FOR THIS PORT, stated so no doc can imply otherwise: everything is Ghidra from zero, which is materially harder than vagrant (CC0 rood-reverse, ~62% matched) or megamanx4 (AGPL sozud/mmx4, byte-identical target). A GENERALISABLE WORKFLOW LESSON, recorded because it cost this find nearly being missed: prior-art.md TS2 entry searched per TITLE, and the artifact is named after a different game — for any new title, search the STUDIO and its sibling titles too.

## What would falsify it

any decomp of SLUS_008.93 surfacing anywhere — a private tree, a non-GitHub host, or a project named after something nobody guessed. The negative cannot see those by construction, so it must always be reported as not found and never as does not exist

## Re-confirmed 2026-08-24 20:18:50

Search refreshed 2026-08-24 across GitHub, decomp indexes, exact SLUS_008.93, and PlayStation-specific terms: no public PSX-executable decomp was found. The newly found 0danny/toy2-decomp explicitly targets PC and supplies no PSX symbols or matching build. This remains a dated search negative.

## Re-confirmed 2026-08-24 20:19:56

After 456a31f, the 2026-08-24 refresh still finds no public PSX SLUS_008.93 decomp; the newly found 0danny project is explicitly PC-only.
