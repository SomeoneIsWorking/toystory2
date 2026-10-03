---
id: 22
title: 24-bit Toy Story 2 FMV present is visibly corrupted
status: fixed
symptom: Exact-pin real run renders legal and ESRB cards correctly, then the first front-end movie shows a mostly black frame with duplicated noisy columns
tags: render,fmv,24-bit,present,runtime,xa,cd
state_items: S007
created: 2026-08-26
updated: 2026-10-02
---

## Observation

An exact-dbdb2baf headless product run captured coherent legal and ESRB screens at presents 300 and 900. After GP1(08) selected 24-bit 256x240 display, present 1500 showed duplicated noisy columns and a mostly black picture. The process remained live and later switched between 24-bit and 15-bit display modes.

Evidence: scratch/screenshots/present_300.ppm, present_900.ppm, present_1500.ppm, and visual montage projection-live-montage.png.

## Static classification

The retained captures are final present-stage images; there is no synchronized raw guest-VRAM dump
and no software-PSX control for the same movie field. The framework CPU VRAM-shot conversion and the
present fragment shader both use the same rule (`display_x * 2 + local_x * 3` bytes), so comparing
those two paths would not independently falsify a shared 24-bit stride error.

The identity-backed FMV overlay does not support a game-side width patch. `0x800D7088` selects 24-bit
MDEC output when its depth argument is 3, allocates `depth << 13` output bytes, and drives the measured
MDEC DMA wrappers at `0x800D9914`/`0x800D9990`. Its completion callback `0x800D6980` uploads one
16-pixel strip at a time through retail `LoadImage` wrapper `0x80085D18`: 24 VRAM halfwords by 240
rows, advancing 24 halfwords until the double-buffered image rectangle spans 480 halfwords (320 RGB
pixels). DMA block counts are exact `0x20`-word multiples. This geometry is internally coherent.

The remaining static coverage gap was at the platform boundary: psxport's guest-visible MDEC pump has
a bit-identical direct-versus-pump differential for varied synthetic 16-bit output, but no equivalent
24-bit control. Static code could not determine whether the corrupt field was already present in source
VRAM or introduced while sampling it.

## Falsifier run (2026-10-02): presentation is exonerated, the disc read is not

**Raw VRAM was captured synchronized with the presented frame and decoded independently.** The
runtime was paused on a movie field (`PSXPORT_VRAMDUMP_AT` with an absolute path), the raw
1024x512 two-byte VRAM image was dumped, and `scratch/mdec/decode_vram.py` rendered candidate
regions without using the shipping shader or CPU-shot formula. The independent decode of the same
VRAM matches the presented image at mean absolute difference **2.49/255**, so the presented picture
is a faithful rendering of what is in VRAM. Nothing in `present.frag` or the shot decoder introduces
the corruption.

**The 3-byte 24-bit pitch is correct, and `x * 3` must not become `x * 6`.** The movie rectangle is
480 halfwords per row = 960 bytes = **320 pixels at 3 bytes/pixel**; the guest uploads it as 20 strips
of 24 halfwords = 16 pixels each (`24 * 3` bytes); and the vendored MDEC's 24-bit row writer emits
`out[i * 3 + 0..2]` for an 8-pixel row, advancing 24 bytes — 3 bytes/pixel. All three agree. The
independent decode at 3 B/px is coherent; at 6 B/px it is not.

**A later 24-bit movie presents perfectly, at both aspects.** Present 275 (the dragon-and-mushroom
picture) is correct in colour, full 320x240 width, centred and pillarboxed at 16:9, and identical to
the 4:3 picture. So the MDEC 24-bit output, its DMA, and the `0x800D6980` strip upload through
`LoadImage` are all correct for a frame that decodes fully. Evidence:
`scratch/mdec/movies43/m_275.png`, `scratch/mdec/movies169/w_275.png`.

**The corrupted first movie is a stalled CD read, not a decode or display fault.** During the first
front-end movie the run logs, repeatedly and escalating to 512 consecutive:

```
[xa:warn] ring FULL (wr=122976 rd=61183) — holding audio sector LBA 13693 (N consecutive);
the SPU pull is not draining CD audio
```

`xa_push_audio_sector` returns -1 when the XA ring is within 4096 frames of full and
`route_audio_to_spu`'s caller then **holds the sector**. The drive stops advancing, so the sectors
the same stream delivers the guest's **video** from never arrive either. The guest's movie loop then
waits on data that will not come, so no display fields advance, so no SPU pulls happen, so the ring
cannot drain — the stall sustains itself. On the healthy side of the stall the imbalance is measured:
production ≈ 12,096 ring frames per display field against ≈ 631 consumed per field.

**A candidate fix was tried and falsified, so it was reverted.** Draining drive-pushed XA frames
regardless of the CdControl clip flag (`CDC_GetCDAudioSample`'s `s_active` gate), and keeping the SPU
advancing while the ring is non-empty (`SpuAudio::frameEx`'s no-consumer early return), both build and
run correctly and change nothing here: `xa_rd` advances before and after, `advanced=1` on 201 of 202
logged fields, and the ring-full numbers are identical pre/post. The ring is already drained while the
guest runs; the deadlock is the guest's own wait. Those hunks were reverted rather than left in a
shared checkout as unproven changes.

Evidence for this section: `scratch/mdec/present_170.png` against `scratch/mdec/dec_170/`, raw dumps
`scratch/mdec/vram_m{100,130,170}.bin`, the run ledger's `[xa:warn]` lines, and a
`PSXPORT_DEBUG=audiofield,xa` run whose per-field lines carry `xa_wr`/`xa_rd`/`xa_pulls`.

## Root cause (2026-10-02, closed): the guest's own player, not the display

CD reads are instant on this port by standing design, and the FMV overlay's player
(`FUN_800D7088` in `FMV/FMV.BIN`, the single callee of the module entry `FUN_800D6628`) is a
streaming loop: it pulls the next sector of a .STR as it decodes and waits on VSync once per movie
frame. Under instant reads that loop asks for sectors far faster than any drive delivers them, floods
the XA audio ring, and the drive's backpressure then holds the audio sector — stopping the disc,
including the sectors carrying that same stream's VIDEO. The guest waits on video, so nothing drains
the ring, so the stall sustains itself.

Two measurements pinned the rate rather than assuming it: the guest issues `Setmode 0xC0` (speed 2,
XA-ADPCM), the drive's own sector period is 225,792 ticks = 33.8688 MHz / 150, and it was delivering
**304.8 sectors/s against the 150 its own Setmode selects** — exactly 2x, because the emulated clock
was charging guest time at the rate the host could execute rather than the field rate. The SPU, paced
by presented fields, stayed at real time; that 2:1 production/consumption imbalance is what filled the
ring. ([emulated_time.cpp](../external/psxport/runtime/psx/emulated_time.cpp) is where that ratio
lives; the clock change that restored drive timing was REVERTED, because instant CD is the standing
design and drive timing does not come back.)

**The fix is the title's, as instant CD requires: the movie is played by psxport's one movie owner.**
`Game::fmv` demuxes the .STR, VLC-decodes the BS, runs Beetle's MDEC at 24-bit depth, presents the
frame, plays the interleaved XA audio on its own host stream and paces video to the media clock. The
guest's player is replaced by a title-owned override (`game/fmv/movie_player.*`) installed when
the FMV image generation is published, which takes the movie from the guest's own argument and returns
exactly what the retail player returns — 0 at end of movie, `_DAT_800A1670` when Start skips, the
word the front-end sequencer treats as "the cold intro is over". See `docs/re-frontier.md` RE-19.

Fmv did not decode 24-bit, so psxport gained `mdec_decode_to_rgb888` (MDEC depth 2: 24-bit blocks are
192 words per 16x16 macroblock, not 128, and 48 bytes per 16-pixel row) and the owner presents those
frames at full colour. Its MDEC drain loop also had to be re-cut: it only counted a stall once the
input was exhausted, so a parked decoder with input outstanding looped forever — reachable at 24-bit,
where 48 output words per 8x8 block park the decoder readily. It hung the player on the second movie
until termination became a function of LACK OF PROGRESS.

Verified: all four intro movies play to their returns at the media clock with no ring warnings, boot
continues through the title screen into Andy's House, and `toy2fmv\dlogo.str` — the movie that
presented as a mostly black frame with fragments — is clean and full-colour at 4:3
(`scratch/mdec/paced43/p120.png`) and centred, pillarboxed at its authored 4:3, at 16:9
(`scratch/mdec/cap169/w70.png`).

A separate open question, not needed for this diagnosis: decoding the disc's own `TOY2FMV/DLOGO.STR`
offline with `psxport/tools/fmv_export/fmv_export` also yields garbage, and a clean-room Python STR
decoder aborts partway on every frame of that stream. The live movie decodes cleanly through the same
shared decoder, so the offline exporter's path or that stream's slice structure needs its own
investigation before it can serve as an oracle for this title.

## Next falsifier

Exercise the skip end-to-end. The override's answer is derived from `Fmv::lastPlaySkipped()` and the
guest's own cold-start word, but a native movie plays as ONE long host turn, so neither the debug
server's `pshot` nor a pad injection can reach it mid-movie — a real finding of its own. Verifying the
skip needs input that can be pressed while that turn runs.

Separately, the native movie present never passes `GpuState::gpu_present_ex`, where
`PSXPORT_PRESENT_SHOT_AT` fires, so `--shot-at` cannot name a movie frame. Arming the existing trigger
from the movie path was tried and reverted: the sink image only lands after the turn ends, so the
capture came back black — a lying instrument is worse than none. Do not patch the image, force 15-bit
mode, or widen the display stride.

## Follow-up: the player is stepped, and the skip is reachable

The first fix replaced the guest's streaming player with one that played a whole movie inside a
single host turn. That verified the decode but made the player unusable: the frame loop, the control
channel and the window's events were starved for the movie's duration, and no input could reach the
pad, so the skip this player implements could not be exercised at all.

`Fmv` is now stepped (`begin`/`step`/`finished`/`skipped`), presenting at most one frame per host
turn, and the identity-scoped title override drives it across turns: one frame, then a
`CooperativeYield` back to itself, leaving the guest call suspended inside the retail player until
the movie ends. Measured, all four movies, in both aspects, identically:

| movie | LBA | bytes | turns |
|---|---|---|---|
| `toy2fmv\acti.str` | 12718 | 4489216 | 440 |
| `toy2fmv\dlogo.str` | 102230 | 4792320 | 158 |
| `toy2fmv\tt.str` | 254106 | 3702784 | 303 |
| `toy2fmv\traler2.str` | 249562 | 9306112 | 454 |

Three defects surfaced from making the skip reachable, each fixed at its owner:

1. **The player resolved input itself, duplicating the owner with different rules.** Polling inside
   the player was a second copy of `Pad::serviceFrame`'s force/hold/REPL resolution — no 32-frame
   force pulse, no `mHoldAt`, consuming `repl_tap_n` outside any frame — and it made a movie skip
   UNREPLAYABLE, because no pad frame was serviced while it ran. A movie host turn IS a presented host
   frame, so the frame driver's `sampleInput()` has already resolved host, forced, replay and
   control-channel input into `buttons` before the guest runs; the player now reads that mask and
   resolves nothing.
2. **The skip waited for an EDGE the pad owner had already taken.** That same per-frame service
   samples the edges, so measured `buttons=FFF7` (Start held) with the edge word still `0000`. The
   skip condition is a LEVEL on the serviced mask, keeping the original player's "already held at open
   needs a release first" contract.
3. **The host's per-field present covered the movie.** The movie presents from inside the guest call,
   before the host's present, so the guest's 2D layers were drawn over it. The driver now presents the
   guest's layers as usual — the ledger sees them delivered — and re-presents the movie on top.

Verified skip end to end: Start pressed through the control channel during `acti.str` reports
`skipped by Start at frame 412` and the guest proceeds to `dlogo.str`.

Verified deterministic, which is exactly what the duplicated resolution broke: that live session was
cut into `replays/toystory2_intro_movie_skip_v1.pad` (78 bytes, 422 frames — the pad reported 412
frames captured at the moment of the press, so movie turns are recorded frames), and replaying that
file alone from boot reproduces the result exactly — `skipped by Start at frame 412`, then `dlogo.str`,
`tt.str`, `traler2.str`. A skip is now an ordinary recorded input like any other.

The decode itself is unchanged by stepping; the per-frame cursors moved into `Fmv`, and the 16-bit
RGB555 branch, `pixels_buf` and `present_rgb555` were removed as unreachable (the depth is always 2).
`mdec_decode_to_rgb555` stays because `tools/fmv_export` and its test still use it.

Remaining falsifier: a native movie frame cannot be seen in a headless capture (issue 40), so movie
pixels are still verified by an independent decode of the same streams, not by a capture.
