---
id: 12
title: CVar audit says PSXPORT_PRESENT_SHOT_AT did nothing while it writes captures
status: open
symptom: Startup warns UNKNOWN knob PSXPORT_PRESENT_SHOT_AT matched nothing and did NOTHING; the same run later writes every requested present_N.ppm
tags: framework,cvar,diagnostics,instrument
created: 2026-08-22
updated: 2026-08-22
---

Observed on the 2026-08-22 Toy Story 2 headless render run with PSXPORT_PRESENT_SHOT_AT=900,1200,1500,1800. The CVar registry does not declare/observe this working diagnostic knob, so the audit's emphatic DID NOTHING verdict is false. The captures themselves remain valid because the same run logs their writes and non-black denominators, but the config audit is not trustworthy for undeclared diagnostic knobs. Proper fix belongs in shared psxport: declare the knob or make the audit distinguish unregistered-but-read from truly unused.
