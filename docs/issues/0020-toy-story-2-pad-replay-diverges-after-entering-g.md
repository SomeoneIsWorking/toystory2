---
id: 20
title: Toy Story 2 pad replay diverges after entering gameplay
status: investigating
symptom: A recorded forced-input run unpauses and moves through Andy's Room, but replaying the unchanged uint16 pad recording reaches gameplay then remains paused at the same late captures
tags: input,replay,determinism,re17,runtime
state_items: S005
created: 2026-08-26
updated: 2026-08-26
---

Recorded run: PSXPORT_FORCE_BUTTONS=BFF7, PSXPORT_FORCE_HOLD=FFDF, PSXPORT_FORCE_HOLD_AT=4968, explicit PAD_RECORD. It reaches Andy's Room, shows pause at present 4700, returns to gameplay at 5000, and the camera moves materially by 5400/6000. Replay run loads all 14,276 samples from the unchanged file and reaches Andy's Room, but is unpaused at 4700 and paused at 5000/5400/6000. Both bounded runs show no watchdog, fatal, or recomp-MISS. This falsifies full replay determinism; RE-17 remains in progress even though real guest input response and visible movement are proven. Compare the actual per-pad-frame masks delivered, guest pause-state transition timing, and any unpaced asynchronous CD/FMV scheduling before changing the replay format.
