---
id: 24
title: Toy Story 2 widescreen proof used unsupported PSXPORT_WIDE knob
status: investigating
symptom: Capability run set PSXPORT_WIDE=1, startup reported it UNKNOWN and doing nothing, and no aspect setting was loaded, so the run remained 4:3 evidence
tags: widescreen,settings,cvar,validation,evidence
state_items: S010
created: 2026-08-26
updated: 2026-08-26
---

The exact psxport `99a42aa3` capability run cannot prove widescreen. Its startup audit reported
`PSXPORT_WIDE` as UNKNOWN, and the shipping framework has no such CVar or legacy reader. Widescreen is
selected through the video settings file instead: `PSXPORT_SETTINGS` chooses the INI path and
`aspect=1` requests 16:9 (`aspect=0` is 4:3). The run used the default `psxport_settings.ini` and no
settings file/aspect evidence was present.

Evidence: `scratch/logs/capabilities-product-99a42aa3.log` lines 5, 21, and 29; shipping settings
contract in `external/psxport/docs/config.md` and loader in
`external/psxport/runtime/recomp/mods.cpp`.

A future run must point `PSXPORT_SETTINGS` at a controlled INI containing `aspect=1`, retain Native
and fps60 requests separately, and capture direct evidence that the loaded aspect and rendered
projection are wide. The corrected invocation fragment is
`PSXPORT_SETTINGS=scratch/config/capabilities-wide.ini`, where that ignored file contains
`aspect=1`. `PSXPORT_WIDE` must not appear. Until then S010 remains missing.
