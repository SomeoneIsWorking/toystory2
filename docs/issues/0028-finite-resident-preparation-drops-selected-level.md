---
id: 28
title: finite resident preparation drops selected level argument
status: resolved
symptom: native FrameDriver retries /LEVEL00/LEVEL.DAT;1 until watchdog aborts in CHD LZMA decompression
tags: frame-loop,cd,abi,re18,S003,S004
created: 2026-08-27
updated: 2026-08-27
---

Affected state: S003, S004.

A bounded retail run after the libgpu timeout owner crossed that boundary, presented two black frames, then repeatedly failed CdSearchFile for /LEVEL00/LEVEL.DAT;1 until the watchdog caught disc_find_file in CHD LZMA decompression. Root cause and resolution pending exact caller analysis.

### Resolution (2026-08-27)
Retail 0x8007AE08 loads levelTable[selection] into a0, stores the same value at 0x800A16A8, and calls 0x8007BEC4 with a0 live. The finite driver stored the level but called its guest bridge without the level argument, whose default forced a0=0. That made 0x8003D88C build absent /LEVEL00/LEVEL.DAT;1 and 0x80082608 retry CdSearchFile forever; repeated ISO directory scans merely placed the watchdog sample in CHD LZMA decompression. The driver now passes the selected level to kResidentReady, and the static frame-owner verifier rejects regression to the dropped argument. Runtime re-verification remains pending a serialized slot.

### Note (2026-08-27)
Bounded retail re20 (PID 2582315) crossed the former LEVEL00 retry: no /LEVEL00/LEVEL.DAT error appeared. It reached selected resident initialization and then the next intentional VSync fatal in 0x80039D9C. This is the required live verification of the selected-level a0 correction.
