---
id: 29
title: pre-resident fade and graphics setup remain guest-blocking
status: resolved
symptom: after selected-level fix, real run aborts at VSync ra 0x8003A078 inside 0x80039D9C called by 0x8007C344
tags: frame-loop,vsync,resident,re18,S003,S004
created: 2026-08-27
updated: 2026-08-27
---

Affected state: S003, S004.

Retail 0x8007C344 owns a 28-field pre-resident fade loop around 0x8003FA68(1), fade/update helpers, and render helper 0x80046A88, then calls the distinct 512x240 graphics initializer 0x80039D9C. That initializer contains two direct VSync calls; the first fatal returned to 0x8003A078 in bounded live run re20. Merely overriding that VSync would collapse the authored loop into one host frame and violate frame ownership.

The source now routes resident preparation through `ResidentPreparation`, a finite owner which transcribes one retail transition iteration per host-owned field and preserves the selected level and playback ABI. It invokes a state-only 0x80039D9C replacement after the 28-field transition, then performs the measured 0x8007BEC4 tail without dispatching either guest blocking owner. The ordinary guest bodies remain reachable through scoped dynarec calls, linked VSync stays fatal, and the finite boundary passes 11/11 (83 checks).

### Resolution (2026-08-27)
Bounded retail re21 (PID 2678458) crossed 0x8007C344/0x80039D9C with no guest VSync or fatal, reconciled all 300 frames with zero dropped layers, and produced visually inspected coherent Andy's Room demo gameplay at presents 45 through 300.
