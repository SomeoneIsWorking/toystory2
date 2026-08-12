---
id: C001
kind: claim
status: holds
created: 2026-08-12
tags: identity,boot
depends: tools/extract_exe.py, docs/info/exe-identity.txt
---

## Claim

Our extracted SLUS_008.93 is sha1 f90c9cd6b4fc9845adfe34e306b7df393bf9154c, 598,016 bytes, and it is the exact binary every recorded measurement in this repo (and the workspace similarity corpus) was made on

## Evidence

MEASURED 2026-08-12. Extracted from /mnt-mounted USA retail CHD (path in the gitignored .env) with psxports own discdump via tools/extract_exe.py: 598,016 bytes, sha1 f90c9cd6b4fc9845adfe34e306b7df393bf9154c. PS-EXE header read from those bytes: magic PS-X EXE present, region tag at 0x4C = Sony Computer Entertainment Inc. for North America area, pc0 = 0x80082D60 (file offset 0x73560), gp0 = 0, t_addr 0x80010000, t_size 0x00091800 (595,968 B), d_addr/d_size/b_addr/b_size all 0, s_addr 0x801FFFF0, s_size 0, sp_gp 0. File size 598,016 = 0x800 header + 595,968 t_size EXACTLY, no trailing slack, which is an internal consistency check on the header rather than an assumption. SYSTEM.CNF (73 bytes, LBA 315) reads BOOT = cdrom:SLUS_008.93;1, TCB = 4, EVENT = 16, STACK = 801FFF00 — note STACK disagrees with the header s_addr and SYSTEM.CNF wins at boot; both are recorded in game/core/game_config.cpp rather than one being picked. CORPUS CROSS-CHECK, which is the part that makes the workspace similarity numbers usable: this extraction is BYTE-FOR-BYTE IDENTICAL to psxport/scratch/lineage2/exes/TOYSTORY2.exe (cmp clean, same sha1, same size), so the recorded 8.2% (TOMBA1) and 6.2% (CRASH2) cells were measured on exactly the binary being ported. THE WEAKNESS, stated: the expectation now lives in docs/info/exe-identity.txt and is OUR OWN measurement. Unlike vagrant and megamanx4 there is no third-party decomp declaring a target hash for this executable, so the check detects difference from what we measured and cannot adjudicate which image is right.

## What would falsify it

any disc yielding a different sha1 for SLUS_008.93 (a different region or revision, or a bad rip) — in which case every measured number in docs/ describes the OTHER image and must be remeasured, not adjusted; also falsified if the workspace corpus file psxport/scratch/lineage2/exes/TOYSTORY2.exe is ever replaced by a different image, since the recorded 8.2%/6.2% similarity figures are keyed to this one
