---
id: I013
kind: instrument
status: trusted
created: 2026-08-22
---

## Instrument

psxport PSXPORT_PRESENT_SHOT_AT headless capture

## Validated by

Validated in both directions on retail Toy Story 2: present 1 reported 0/691200 non-black before guest output, while presents 30/120 reported 109116/691200 and present 900 reported 232110/691200; converted captures visibly show legal text and the ESRB card. It measures the headless sink after present, not guest OT attribution.

Known diagnostic defect: issue #12 records that psxport's CVar startup audit calls this knob UNKNOWN and
says it did nothing even in the same run that later writes the requested captures. Trust the explicit
`[present_shot] wrote ...` result and file, not that registry warning, until the shared audit declares or
observes the knob correctly.

## Known failure modes

(none recorded yet)
