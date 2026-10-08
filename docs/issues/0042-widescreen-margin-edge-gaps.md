---
id: 42
title: 16:9 margins show gaps at the outer edges
status: open
symptom: Black or dark wedges where floor or wall should continue at the far left and right of the 684-wide canvas
tags: widescreen,render
state_items: S010
created: 2026-10-07
updated: 2026-10-07
---

On the record path at 16:9 (`scratch/record/rec169/present_{1400,1990}.png`), the outer edges of the
margins sometimes show no geometry. Examples are the floor at the right edge and a dark block at the left
in Andy's House. The old Gte-path baseline (`scratch/record/base169/present_1990.png`) shows the same
kind of gap.

`ResidentWidescreenCull` widens the visibility window (`0x80027AF0`) and the published screen rectangle
(`0x80010000`). It is not yet known whether the gaps are a third cull test or the guest's
`rgb(32,32,32)` backdrop where the room geometry ends.

**Next step**: decompile the per-face screen test in `0x800100E4` and in the object-renderer drawers.
Then compare one gap's faces against that test.
