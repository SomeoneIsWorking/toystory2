# References — the prior art on this engine, and exactly what we may take from it

The workspace-wide survey is `external/psxport/docs/prior-art.md`. This file is what is specific to Toy
Story 2, and it corrects two omissions in that survey (the licence column, and a decomp of a sibling
title it did not find).

**The standing rule governs everything below: where a reference and a MEASUREMENT disagree, the
measurement wins.** And a second rule that this title needs and the sibling ports do not: **every source
must be labelled with the PLATFORM it came from before it is believed.**

## THERE IS NO DECOMPILATION OF THIS GAME, and here is how that negative was established

`docs/info/claims/006-*` is the claim; this is the short form.

Searched 2026-08-12: `gh api` on every candidate repo plus its full file tree and README; `gh search
repos` over toystory2 / "toy story 2" / "buzz lightyear" / "travellers tales" / "toy story racer" /
"bugs life playstation" / "muppet race mania" / "weakest link psx" / "rascal psx" / "RNC ProPack" / "Rob
Northen compression" / EpicMinecartz / psx-modding-toolchain; the complete 17-repo listing of
`users/mateusfavarin`; a full read of `decomp.dev?platform=ps1`; and two web searches including the
engine author's name (Dave Dootson). **decomp.dev lists nothing for Toy Story 2 and nothing for any
Traveller's Tales title. The GitHub search DID find one decomp of a SIBLING TT title — `mateusfavarin/tsr`
(Toy Story Racer, `SLUS_012.14`, MIT), in the table below and in `docs/issues/0004-*` — but nothing for
`SLUS_008.93` itself.** Scoped that way deliberately: the unscoped "nothing for any TT title at all" is
exactly the sentence issue 0004 exists to stop being re-derived.

Search refreshed 2026-08-24 over GitHub, decomp indexes, the exact `SLUS_008.93` serial, and the
PlayStation-specific title terms. It still found no decomp of the PSX executable. It did find the new
`0danny/toy2-decomp`, but that project explicitly targets the **PC version**, so it does not supply
PSX symbols, function boundaries, or a matching build. This remains a dated search negative, not a
claim that a PSX decomp cannot exist.

**A SEARCH NEGATIVE IS WEAKER THAN A MEASUREMENT, and this one must always be reported as "not
found".** It cannot see a private tree, a non-GitHub host, or a project named after something nobody
guessed. What raises confidence without proving anything: the community's centre of gravity is visibly
elsewhere — every active TS2 project targets the **PC** build (RibShark/ToyStory2Fix, ManiacKnight,
JeffRuLz, mouksx/t2gm2), and `cheeseandcereal` scopes itself explicitly to "PC US, PC UK, Project64".
No public PSX-executable decomp was found by either search.

**The consequence, stated plainly:** no symbol map, no function boundaries, no matching build for
`SLUS_008.93`. Everything is Ghidra from zero (`docs/re-frontier.md` RE-00). That is materially harder
than `vagrant` (CC0 `rood-reverse`, ~62% matched, 21/21 byte-identical module targets) or `megamanx4`
(AGPL `sozud/mmx4`, declared build target byte-identical to our extraction, so its symbols ARE our
addresses). Nothing in this tree may imply a head start it does not have.

## What DOES exist — with licences, and with what it does and does not buy

| project | licence | platform | what it buys us |
|---|---|---|---|
| [mateusfavarin/tsr](https://github.com/mateusfavarin/tsr) — decomp of **Toy Story Racer** (`SLUS_012.14`) | **MIT** — code and ideas may be taken freely | PSX, a DIFFERENT game | The only decomp-shaped artifact in the TT PS1 space. `docs/DAT.MD`, `RAW.MD`, `AXE.MD` (struct-level format docs derived from real decompiled loaders); decompiled C for engine primitives (`geom.c` GTE transforms, `heap.c`, `sprite.c`, `axe.c` chunk-stream mesh parser); `symbols/variables.txt` (23 globals) and `docs/FUNCTIONS.MD` (14 functions with addresses); working `extract_dat.py` / `extract_raw.py`. **Its addresses are SLUS_012.14's, with NO translation to ours.** |
| [juanmv94/TravellersTalesPSXCollisionViewer](https://github.com/juanmv94/TravellersTalesPSXCollisionViewer) | **NO LICENCE AT ALL** (GitHub reports `license: null`; the tree holds only README.md) → all rights reserved → **READ AND LEARN ONLY** | PSX, TS2 among its targets | The single best PSX-side engine document in existence for this game: a 14 KB README covering all 7 TT PSX titles, naming the engine's author, documenting the collision object/mesh layout, the scene-item struct, vertex/face encoding, face flags, CLUT/VRAM-page packing, translucency modes, LOD, sprites and backgrounds — **with per-game deltas**. VERIFIED: its own sample `util/RAWfiles/LEVEL05_LEVEL1_MOD.RAW` corresponds to our disc's `LEVEL05/LEVEL1.RAW`. |
| [mouksx/Toy-Story-2-Modding](https://github.com/mouksx/Toy-Story-2-Modding) | **NO LICENCE AT ALL** → read-only; also ~160 MB of extracted copyrighted assets, independently disqualifying under the never-commit-assets rule | mixed: its `.RAW`/`.DAT` half is PSX, its `.NGN` half is NOT | Its own README disclaims most of its content as four years old and unreliable "except the NGN section" — and the NGN section is precisely the part that does not apply to us. MEASURED CROSS-CONFIRMATION of the usable half: its `1.Andys House/level.raw` is byte-length-identical to our `LEVEL01/LEVEL.RAW` and its 39 sub-file lengths match our 39 decoded chunk lengths exactly and in order. |
| [temisu/ancient](https://github.com/temisu/ancient) | **BSD-2-Clause** — the only cleanly-licensed code in this whole field | platform-agnostic codec | `src/RNCDecompressor.{cpp,hpp}` (468 lines) supporting new AND old RNC1 and RNC2, with committed test vectors. Was the candidate for RE-08; MEASURED 2026-08-24 to be irrelevant — TS2 `.RAW` is not RNC (see `docs/info/claims/019-*` and the vendoring paragraph below). Still the first call if an actual RNC container ever appears in this engine's files. |
| [lab313ru/rnc_propack_source](https://github.com/lab313ru/rnc_propack_source) | no SPDX licence; a bare **written permission grant** verified at source in its `LICENSING` file ("Rob Northen knows about this project, and has no claims about it… you may use this source code in any commercial\|no commercial project") — permissive-BY-GRANT, **not** an OSI licence from the rights holder | codec | A working C implementation of both RNC methods (pack and unpack), useful as a REFERENCE and a cross-check oracle. Prefer `ancient` for anything shipped: this is a decompilation of the proprietary tool, so the grant is the author's assurance rather than a licence. |
| [cheeseandcereal/ToyStory2Resources](https://github.com/cheeseandcereal/ToyStory2Resources) | NO LICENCE | **PC + N64 only** | Nothing. A LiveSplit autosplitter and two Cheat Engine tables. Recorded as a dead end so nobody reopens it. |
| [EpicMinecartz/ToyTwoToolbox](https://github.com/EpicMinecartz/ToyTwoToolbox) (+ `Toy2LevelDump`, GPL-3.0) | ToyTwoToolbox: NO LICENCE | **the `.NGN` data build — NOT PSX** | **Nothing, and this is a measured negative rather than a judgement.** See the trap below. |

## Vendored here: NOTHING beyond the framework, and that is a decision

- **No decomp submodule, because none exists.** `external/` holds `psxport` and nothing else.
- **`mateusfavarin/tsr` is MIT and could legally be vendored — deliberately NOT.** A submodule sitting
  where `vagrant`'s `rood-reverse` and `megamanx4`'s `sozud/mmx4` sit would misrepresent this tree: it
  decompiles a DIFFERENT game's executable and its 14 functions' addresses do not translate to
  `SLUS_008.93` by any offset. It is cited here as the primary external RE source; clone it into the
  gitignored `scratch/` when someone is actually working the format layer. (If a later session disagrees
  and vendors it, the directory MUST be named so nobody can mistake it for a TS2 decomp, and the first
  sentence of its entry must state the address-translation gap.) Two further limits: its `docs/*.MD`
  cite `research/ghidra/exe.c`, which is gitignored and absent, so those docs cannot be audited against
  the code they came from; and it is a one-maintainer project at 14/339 functions — do not plan around
  it growing.
- **`temisu/ancient` (BSD-2-Clause) — MEASURED IRRELEVANT TO THIS CONTAINER (2026-08-24, RE-08).**
  The recorded decision was "worth vendoring, deferred until RE-08 needs it". RE-08 then measured that
  TS2 `.RAW` payloads are NOT RNC in any variant (new/old × method 1/2 all fail; see
  `docs/info/claims/019-*`): the codec is Traveller's Tales' own `DecompressRAW` LZ scheme, for which
  ancient has no code. Its SHAPE was exactly what the falsification harnesses needed (`RNC1/2-new` +
  `RNCDecompressOld` ports in gitignored `scratch/raw/`), which is what closed the question without a
  vendor. **Nothing vendored; the deferral is resolved as "not needed", not "still pending".** If a
  future container turns out to BE RNC (some TT titles used it elsewhere), revisit — it remains the
  only cleanly-licensed codec code in the field.
- **The `.RAW` codec reference is [mateusfavarin/tsr](https://github.com/mateusfavarin/tsr) (MIT)** —
  its `scripts/extract_raw.py::decompress_raw_chunk` transcribes the game's own `DecompressRAW`
  (Ghidra-derived), and docs/RAW.MD documents the 14-byte-header/sentinel stream we confirmed.
  MIT makes it vendorable with attribution if the native asset path ever wants C++; today's consumer
  is the Python instrument, so the SHAPE was taken and cited (`tools/raw_unpack.py`) per this file's
  standing rule.
- **May not be vendored at all:** `juanmv94/...` and `mouksx/...` (no licence; and juanmv94 additionally
  embeds a 993-byte blob of proprietary x86 machine code lifted from the RNC ProPack DOS tool and
  executed through a function pointer, submodules an unlicensed mirror of that tool, and commits 54 ×
  4.2 MB emulator savestates that are full RAM images of copyrighted games), `cheeseandcereal`,
  `EpicMinecartz/ToyTwoToolbox`. `Toy2LevelDump` is GPL-3.0 and therefore vendorable, and useless.

## THE TRAP: `.NGN` is not on the PSX disc, and the best-looking tool in the field is built on it

`mouksx`'s README calls NGN "the first of the 3 data files needed to load a level" and says it holds
character models, animations, level geometry, textures, area portals and shape links. The largest TS2
tool in existence (`EpicMinecartz/ToyTwoToolbox`, ~11 MB, a full C# editor with
Character/Animation/AreaPortal/Geometry/Material classes) is built entirely around it.

**MEASURED and falsified: `grep -ci NGN` over the 300 filesystem entries this tree's own instrument
returns (`python3 tools/discdump.py list`, which prints that denominator itself) → 0 of 300.** An earlier
"303 of 303" here was a `wc -l` of that listing, counting its blank line and two `[discdump]` trailer
lines as entries; the conclusion is unchanged, the denominator was not the tool's. The obvious
blind spot was closed too — grepping for an `NGN` magic inside the extracted `LEVEL.RAW` (469,276 B),
`LEVEL.DAT` (437,164 B) and `TERRAIN.ALL` (116,368 B) gives 0 hits in all three. On PSX that content is
split across `.ALL` / `.ANM` / `.DAT` / `.BIN`. **Residual blind spot, stated:** 3 of 46 `.RAW` files were
grepped and no `.BIN`/`.ANM`/`.STR` were, so "no NGN in the ISO9660 filesystem" is fully closed while "no
NGN anywhere in any container" is not.

So NGN belongs to the PC (and/or N64) data build, and every NGN-based tool and note describes a different
platform's build. **This is written here specifically because the wrong tool is the best-looking one.**

## The standing hazard of this title: cross-platform contamination

TS2 shipped on PS1, N64, PC and Dreamcast, and the community lives on the PC build. Only juanmv94 (from
PSX savestates) and mouksx's `.RAW`/`.DAT` half are PSX-derived. A PC-derived address, struct size or
offset imported here would look exactly like a working fact — which is the failure mode the no-bandaids
rule exists to prevent. Label every source with its platform before believing it.

## What legitimately transfers from `tsr`, and how far

**NAMES, STRUCTURES and MECHANISMS — never addresses.** The strongest single piece of evidence in this
field is a field-exact agreement between two REs that did not collaborate, on two different games:
juanmv94 (from TS2 savestates) describes the 32-byte scene item as s32 x/y/z, s16 rot x/y/z, s16 scale
x/y/z, ending in a u32 object pointer, and asserts the structure is identical across the games; tsr's
`docs/DAT.MD` (from decompiled TSR loaders) gives `DatObjectTransformDef`, size 0x20: `0x00` s32
posX/Y/Z, `0x0C` s16 rotX/Y/Z, `0x12` s16 scaleX/Y/Z, `0x18` unk, `0x1C` u32 offMeshStream. Offset for
offset. Shared conventions worth writing down: Euler rotation in PSX angle units, fixed-point scale where
`0x1000 == 1.0`, and DAT positions stored in HALF world units.

That is strong evidence about the ENGINE FAMILY and **zero** evidence about `SLUS_008.93` specifically —
`docs/re-frontier.md` RE-09 keeps it as a hypothesis to confirm against our bytes, and records the
documented per-game deltas that prove the family is not uniform. One delta is INSIDE this game: the TS2
demo's object struct is 4 bytes smaller than the retail one. **Pin the retail USA build and never mix
notes across builds.**
