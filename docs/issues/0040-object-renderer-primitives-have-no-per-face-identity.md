---
id: 40
title: Object-renderer primitives have no per-face identity at the guest boundary
status: investigating
symptom: Buzz, actors, pickups and hierarchical models are drawn as the newer record in a 60 fps in-between
tags: interpolation,render,producers
state_items: S009,S011
created: 2026-10-07
updated: 2026-10-07
---

On the record path only keyed primitives move in an in-between. In Andy's House (route
`replays/toystory2_player_andys_house_v2.pad`, frames 1400..2000, REPL `census`) about 236k of 714k
gameplay primitives come from the resident update `0x8007B254` with no key, about 335 per frame from the
object renderer alone.

- The object renderer `0x8002518C(object)` loops over the parts at `0x800A9618 + i*0x6C` (model
  `*(0x800C7268 + type*4)`, part range `model+4..model+6`) and calls the drawers `0x8001CD34`,
  `0x8001C920`, `0x80031C3C`, `0x80033C30`, `0x800396B0`, `0x8003925C`, `0x8001D240`, `0x800362C0`,
  `0x8001DFE4` and `0x80033C58`.
- The hierarchical drawers are `0x8002B6F0` (about 30k primitives) and `0x8002AC40` (about 1.2k).

Each drawer takes packets one after another from the pool cursor `0x800A1608` and walks its whole face
list with GTE work carried across iterations. A face that is culled consumes no packet, so the packet
index is not the face index, and no guest word names the face a packet belongs to. Keying by packet order
or count is forbidden.

**Proper fix**: port each drawer natively, so that each packet is written inside
`Guard(core.emission, drawer, object, part * faces + face)` with the face index the native loop owns.
Then prove each port byte-for-byte with recordcheck at 4:3.

## Progress

- `ActorIncarnations` observes the guest's actor reset `0x8006A9FC` (level start and respawn), so a
  respawned pool slot is a new object key.
- `ActorProducers` overrides `0x8002518C` and `0x80024174` to note the drawn actor; it opens no scope,
  because guest stores of the unported drawers would otherwise bind to one bare
  `(renderer, actor, 0)` key (13,047 duplicates on the route before this was fixed).
- `PartFaceDrawers` ports `0x8001C920` and `0x8001DFE4`; each face is keyed
  `(0x8002518C, part object, 0)`, the part object being `(a0 part record, incarnation)`. Keying tests: `actor_faces_are_keyed_by_model_face_not_packet_order`,
  `a_reset_actor_gets_a_new_key`,
  `unported_drawer_packets_stay_unkeyed`.
- Andy's House, frames 1400..2000: keyed 503,291 of 713,669 (70.5%, was 43.9%), 0 duplicate keys; the
  actor renderer keys 190,312 and leaves 11,064 unkeyed. 4:3 recordcheck mismatched=0 over 2006 presents;
  screens and scratchpad identical to HEAD; RAM differs only in the guest profiler's timing words.

Open: `0x80031C3C`, `0x800396B0`, `0x8003925C`, `0x8001D240`, `0x800362C0`, `0x80033C58`, and the
hierarchical drawers `0x8002B6F0`/`0x8002AC40` (RE-25).

## State producer

`0x8002518C` is a state producer: `PartDrawRecorder::save` keeps each part draw's inputs and
`PartStateRender::render` re-runs the same native drawer body on control registers blended at t, over host
memory. Tests: `prelit_` and `normal_lit_render_at_one_reproduces_the_guest_packets_with_memory_scrambled`
and the `*_render_halfway_equals_a_frame_drawn_with_*_halfway` cases in `resident_producers_boundary`. Override differential over frames 0..1600 of the
route: `0x8001C920` 506 of 506 sampled calls match, `0x8001DFE4` 319 of 319; `0x8001CD34` (DCPL-lit) is called by none of the route replays
(Andy's House player, pause and area transition, exit level, intro, movie viewer; override differential), so it stays on the guest and its
parts are drawn unkeyed; it needs a route that reaches part type 1 mode 0 to be ported. Recordcheck at 1x 4:3: fps60 off and on both 2006 presents, 0 mismatched.
Keyed blend is still needed for the unported drawers listed above and for the slot meshes.
