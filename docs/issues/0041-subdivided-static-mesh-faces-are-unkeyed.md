---
id: 41
title: Subdivided static-mesh faces are unkeyed
status: open
symptom: Near-camera floor and furniture faces do not interpolate while far faces of the same mesh do
tags: interpolation,render,producers
state_items: S011
created: 2026-10-07
updated: 2026-10-07
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

**Proper fix**: port the subdivider natively and key each sub-packet by
`(submitter, slot table, entry)`, with the quadtree path as the part.

The actor drawers' approach (issue 40) applies: a native port that writes each sub-packet inside a full
key opened by the port itself, never a dispatcher scope. Not started.
