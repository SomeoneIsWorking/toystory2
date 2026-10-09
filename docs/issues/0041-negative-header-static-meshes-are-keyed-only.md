---
id: 41
title: Negative-header static meshes are keyed-only
status: open
symptom: Static meshes with auxiliary vertex records (negative header) are keyed-only; near-camera subdivided faces of positive-header meshes are fixed
tags: interpolation,render,producers
state_items: S011
created: 2026-10-07
updated: 2026-10-09
---

The static mesh submitter `0x800100E4` keeps one packet per face in the slot array, and
`SlotMeshProducers` keys that packet. A face flagged for subdivision (byte `+0x27` of its packet) is
expanded instead:

- `0x80017B34` hands it to the recursive quadtree subdivider `0x80017B6C`.
- The clip paths go through `0x80016690` and `0x80014D3C`.

The sub-packets come from a sequential cursor, and their number depends on distance. On the Andy's
House route that is about 266 primitives per frame. These sub-packets stay unkeyed and are drawn as the
newer record.

Before this was understood, the dispatcher's producer scope gave these packets the key `(P, mesh, 0)`.
The tap then numbered them consecutively, which paired them by order and split the table legs in the
in-betweens. `SlotMeshProducers` no longer opens a dispatcher scope (test
`packets_outside_the_slot_array_stay_unkeyed`).

## Done

Both bodies are native and render from saved state (`game/render/rigid_mesh_drawer.*`,
`static_mesh_drawer.*`, the four subdividers over `split_engine.*`, `slot_packets.*`, `slot_release.*`),
so subdivided sub-packets are produced inside the object's key and interpolate with it. Override diff
over the four routes under `replays/`: 0 mismatches (static 0x800100E4 and rigid 0x80017FF8 sampled on every call).

## Remaining

The static submitter's negative-header meshes (header word <= 0: auxiliary vertex records, a prologue that
normalises the rotation rows and fills the light control registers, then command 12 `0x80012FE0`, the
reflective quad, and command 0) are not ported. `MeshDrawers::plan` refuses them and the call runs on the
guest with `keyFaces` keying its slot-array packets: 2,342 calls over 2,500 frames of the Andy's House
route, 6 double-packet primitives and 4 quads each. They interpolate through psxport's `keyedBlend` only.

**Proper fix**: port the prologue (the `0x80083944` normaliser, the `0x800B27A8` cache clear, the control
register writes) and handler `0x80012FE0` in the static walk, extend `Gather` with the cache range, prove with
the override diff.
