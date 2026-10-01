---
id: 35
title: Toy Story 2 loading census - every CD read issuer, the wait it drives, and which waits are loading-only
status: open
symptom: S015 said "no load operation has been censused or classified for Toy Story 2", so the first implementation step would have been a guess about which of Toy Story 2's waits are I/O, which are authored presentation, and which the port already removes
state_items: S015
tags: loading,cd,iso9660,overlay,census,re,transition,fmv,pad-cancel
created: 2026-10-01
updated: 2026-10-01
---

## What this is, and what it is not

A census of Toy Story 2's load operations and of the waits they drive, plus one measured run. **No
product code was changed and no psxport commit was made** (`external/psxport` was read at its pin
`d51ee05a`; a first build attempt mis-pointed `PSXPORT_DIR` at the shared checkout HEAD `72bd5293` and
is called out in section 8, because it produced a run that *looked* plausible and was not).

Every address below was read from `SLUS_008.93` (598,016 bytes, `sha1 f90c9cd6b4fc9845adfe34e306b7df393bf9154c`,
the identity in `docs/info/exe-identity.txt`) placed the PS-X EXE way by `tools/ram_image.py`
(`t_addr 0x80010000`, `t_size 0x91800` -> 148,992 text words at `[0x80010000,0x800A1800)`), and from the
provisioned module images placed at their **proven** guest bases: `LEVEL*.BIN` at `0x800D12C0`,
`BITS/MEMORY.BIN` and `FMV/FMV.BIN` at the shared slot `0x800D5D20` (`tools/overlay_map.py`, and the
constant `kLevelOverlayBase`/`kMemoryOverlayBase` in `game/core/guest_facts.h`). Disassembly is
`external/psxport/tools/disasm.py` over a 2 MiB RAM image built that way; every range quoted below
decoded with 0 unknown words except where an UNKNOWN is quoted on purpose (section 8). There is no
decompilation of this title, so **every** name below is a role inferred from bytes, not a recovered
symbol.

Evidence tags:

* **[B]** read from instruction words or image bytes in this session; the word or address is quoted.
* **[S]** read from this repository's own sources.
* **[I]** inferred; named as such, with what would settle it.

## Headline

1. **Toy Story 2 has ONE blocking read primitive, and it is 22 instructions long.**
   `0x80082608` is `CdControlSync(CdlSetmode)` -> `CdSearchFile` -> `CdControlSync` -> **`CdRead` of the
   whole file in one call** -> `CdReadySync(0)` in a spin. Everything the game loads goes through it,
   and every other CD read in the title is inside `FMV/FMV.BIN`'s STR streamer. There is no
   cooperative/streaming file loader and no stage machine: **[B]**, section 1.
2. **The census is closed, with denominators.** 13 `jal` sites of the file loader `0x80082508`,
   1 of `0x80082728`, 1 of `0x80082608`, 7 `jal` sites of `CdRead 0x80093AF0` (2 boot, 5 in FMV.BIN),
   7 of `CdReadySync 0x80093BF4`, 4 of `CdSearchFile 0x80092AE8`, and **0** `lui`-materialised and
   **0** raw-word references to any of them over 148,992 boot words plus every placed module. The same
   scan finds 4 and 3 materialisations of the two slot bases, so the zeros are a measurement.
3. **The biggest loading-only wait in the game is `FMV/FMV.BIN`, read whole, five times, before the
   four intro movies and once before the Level 1 intro.** 510,960 bytes each, `0x8003EEAC` and
   `0x800417D0`, both to `0x800D5D20`. **[B]**
4. **Two unbounded loops exist in the load path and one of them calls guest VSync.**
   `0x80082750`/`0x8008276C` re-issue the whole read while the result is `<= 0` or while `[$gp+0x8D0] != 1`
   (this is the retry that issue 0028 hit), and `0x8007F174` spins `VSync(0)` forever behind a `break`
   when a `.vh`/`.vb` file is `>= 0x4001` bytes. **[B]**
5. **The intro movies DO have a recovered retail cancel, and it is not the one the goal text assumes.**
   `FMV.BIN` has exactly one pad-mask site, `0x800D76EC`: `andi $v0,$v1,8` (**Start**) skips
   unconditionally; `andi $v0,$v1,0x4000` (**Cross**) and `andi $v0,$v1,0xF000` (any face button) skip
   only when `[0x800A1670] != 0`. There is no pad read in FMV.BIN's five read sites. **[B]**
6. **No loader reads the pad.** 28 `lhu` sites of the mask word `[0x800A1480]` exist in the boot image,
   in exactly **10** enclosing functions, and none of those ten is a loader; FMV.BIN has 1, MEMORY.BIN
   94 (all UI), LEVEL.BIN 0. **[B]** So every loading-only wait in this title is uncancellable in
   retail - which is what makes the removal design below a *purpose-built* transition, not a recovered one.
7. **Measured, one headless route** (`tools/verify_route.py --route`, psxport `d51ee05a`): 37
   `CdRead` of 2,652 sectors (5,431,296 B) and 32 `CdSearchFile`; every whole-file blocking window sums
   to **764 ms** of a 17 s run, of which the five FMV overlay loads are 53 ms. The four intro movies
   are **194 / 41 / 73 / 69 display fields**. The one genuinely load-bound CPU wait is the RAW decode
   (28 slices / 15,245,664 cycles cold, 60 slices / 33,309,042 cycles entering Level 1), and it is CPU,
   not I/O. Section 5.

## 1. The CD primitives this title uses (all [B])

### 1.1 The file loader, three functions

| address | role | bytes that establish it |
|---|---|---|
| `0x80082508` | `LoadFile(char *name, void *dest)` - the title's only whole-file loader | `0x80082510 move $s0,$a0` / `0x80082518 move $s1,$a1`; `0x8008251C-0x80082520 lui $a1,0x800a; addiu $a1,0x1048` (the drive prefix); `0x80082528 jal 0x80082e3c` + `0x80082534 jal 0x80082e1c` (sprintf + strcpy into `sp+0x10`); uppercases the name in place (`0x80082568 addiu $v0,$a0,-0x61` / `0x8008256c sltiu $v0,$v0,0x1a` / `0x80082578 sb $v0,($v1)`); prepends the drive (`0x8008259c-0x800825a4`, bytes from `$gp+0x378/0x379`); appends `;1` (`0x800825c0-0x800825c4`, format `0x800A1054`); `0x800825d8 jal 0x80082728`; returns `[0x800825e0] + 1` as the size and bumps a load counter at `[gp+0x36c]` |
| `0x80082728` | the **unbounded retry wrapper**: `LoadFileRetry(path, dest)` | `0x80082748 sw $zero,0x8d0($gp)`; loop head `0x8008274c`/`0x80082750 jal 0x80082608`; `0x8008275c blez $v1,0x80082750` (retry while the read returned `<= 0`); `0x80082764 lw $v0,0x8d0($gp)` / `0x8008276c beq $v0,$s2,0x80082750` with `$s2 = 1` (`0x80082740`) - **retry forever until `[$gp+0x8D0] == 1`**. No counter, no bound, no VSync, no draw |
| `0x80082608` | the read body, and the whole blocking wait | `0x80082624 addiu $a0,$zero,0xb` + `0x8008263c jal 0x80090d40` = `CdControlSync(CdlSetmode)`; `0x80082644 addiu $a0,sp,0x10` / `0x80082648 jal 0x80092ae8` (`CdSearchFile`) with `0x80082650 beqz $v0,0x80082648` (**retry loop**); `0x80082658 lw $s0,0x14($sp)` = the returned LBA's 32-bit word at location+4; size 0 -> error `0x15` (`0x80082664`); `0x80082670 addiu $a1,sp,0x10` / `0x80082674 jal 0x80090fa4` (the sync command sender); sector count `$s1 = (size + 0x7ff) >> 11` (`0x80082680-0x80082694`), tail copied by `0x80082e6c`; **`0x800826b4-0x800826c0 move $a0,$s1; move $a1,$s3; jal 0x80093af0` with `$a2 = $s2 \| 0x80`** = one `CdRead` of the whole file; **`0x800826d4 jal 0x80093bf4` with `$a1 = 0` and `0x800826dc bgtz $v0,0x800826d4`** = `CdReadySync(0)` in a spin until it returns `<= 0`; returns `$s0`, the exact CdlFILE size |

There is no `CdReadyCallback` registration in this path and no `CdSync` after the read. The wait is
`CdReadySync` and nothing else.

### 1.2 The libcd layer the title reaches

| address | role | how established |
|---|---|---|
| `0x80091DE4` | the four-argument stock command sender | the body at `0x80090DCC`/`0x80090DF4`/`0x80090E18` and its three twins `0x80090F24`, `0x80091050`, `0x80091074`, `0x80092608..0x80092660` all `jal` it. **Every caller is inside libcd** - 13 sites, 0 outside `[0x80090D00,0x80092680)` |
| `0x80091898` | the sync poll/callback dispatcher | 4 callers (`0x80090CD8`, `0x800910A4`, `0x80091EA8`, `0x80092674`), all libcd. The title never calls it directly |
| `0x80090D40` | `CdControlAsync(control, param, result)` - issues and returns | `0x80090D60 addiu $s0,$zero,3` (three tries) / `0x80090D68 andi $s3,$s4,0xff` (control) / `jal 0x80091DE4` at `0x80090DCC`, `0x80090DF4`, `0x80090E18` / `0x80090E24 addiu $v0,$s7,1` returns a count, and there is **no** call to `0x80091898` |
| `0x80090FA4` | `CdControlSync(control, param, result)` - issues and waits | the same body, and `0x800910A4 jal 0x80091898` |
| `0x80090E78` | a third control variant, used only by the STR streamer | the same body again; **20 callers**: 15 in the boot image, 5 in FMV.BIN |
| `0x8009118C` | `CdIntToPos` | `0x8009118c lui $v1,0x1b4e` / `0x80091190 ori $v1,$v1,0x81b5` (the multiply constant), 6 callers, all libcd |
| `0x80092AE8` | `CdSearchFile(char *path, CdlLOC *loc)` | `0x80092b40 lbu $v1,($s3)` / `0x80092b44 addiu $v0,$zero,0x5c` / `0x80092b48 beq $v1,$v0` - the root marker test. 4 callers: `0x80081FE0`, `0x800820BC`, `0x800823E0`, and the loader at `0x80082648` |
| `0x80093AF0` | `CdRead(sectors, dest, mode)` | 7 `jal` sites (section 2.4) |
| `0x80093BF4` | `CdReadySync(mode)` | 7 `jal` sites, one per `CdRead` |
| `0x80091108` | `CdGetSector` | `0x80091110 jal 0x80092804`; 3 callers (`0x80082AB4`, `0x80093510`, `0x800935A0`). This is the **data-path** read and the measured run exercises it: `[cd] CdGetSector 8 words -> 0x800BD9C0 (sector LBA 2236, cursor 44/2352)` |
| `0x80093408` | libcd's own `CdControlSync(CdlRead)` read path (`0x80093424 jal 0x8009118c`, `0x80093434 jal 0x80090d40`, `0x80093444 jal 0x80093af0`, `0x80093450 jal 0x80093bf4`) | 3 callers, **all inside libcd** (`0x80092E04`, `0x80092EB0`, `0x800931BC`). The title never reaches it - see the null and its control in section 2.5 |
| `0x80082008` | `FileSize(char *name)` - the same path-building shape as `0x80082508` (`0x80082020 jal 0x80082e3c`, `0x80082030 jal 0x80082e1c`, format `0x800A1048` + `0x800A1054`) | that it returns a *size* is [I], from its only interesting caller `0x8007F154`, which masks the result with `0xFFFFF800` (`0x8007F160-0x8007F164`) and rejects anything `>= 0x4001` |
| `0x80082790`, `0x80082e1c`, `0x80082e3c`, `0x80082e6c`, `0x80082e7c`, `0x80082e9c` | `itoa`, `strcpy`, `sprintf`, `memcpy`, `memset`, `strlen` by body shape | not load primitives |

### 1.3 The FMV STR streamer's five reads (all in `FMV/FMV.BIN`)

Every one is preceded by the same three instructions - `jal 0x800d8594`, `$a0 = 0x15`
(**`CdlReadS`**, XA-style streaming), `jal 0x80090e78`, then `jal 0x800893b4` with
`$a0 = [0x800FB8E8]` - and is followed by `CdReadySync`.

| site | `CdRead` | wait | notes |
|---|---|---|---|
| `0x800D879C` | 1 sector -> `sp+0x10` (`0x800D8794 addiu $a0,$zero,1`, `0x800D8798 addiu $a1,sp,0x10`, `$a2 = 0x80`) | single `CdReadySync` at `0x800D87A8`, **no loop** | per-frame audio/video sector |
| `0x800D8864` | `(size+0x7ff)>>11` sectors -> `[0x800FB900]` | single `CdReadySync` at `0x800D8870` | chunk read |
| `0x800D8C04` | 4 sectors -> `0x80159008` | single `CdReadySync` at `0x800D8C10` | |
| `0x800D8D18` | 4 sectors -> `0x80159008` | single `CdReadySync` at `0x800D8D24` | |
| `0x800D91C4` | `$s0` sectors -> `$s3` | **`0x800D91D0 jal 0x80093bf4` with `0x800D91D8 bgtz $v0,0x800D91D0`** - an unbounded spin | the only unbounded wait inside FMV.BIN |

`0x800D6628` is the FMV entry: it stores the movie index at `0x80152908` (`0x800D6654`), calls
`0x800D7938`, then `0x800D86AC`, then dispatches through a **29-entry** table at `0x800D5EDC`
(`0x800D6670 sltiu $v0,$s0,0x1d`; `0x800D667c-0x800D6688 lui $at,0x800d; addu; lw $v0,0x5edc($at)`;
`0x800D668c jr $v0`). Its only two callers are `0x8003EEC4` and `0x800417D8`.

## 2. The issuer census

**Method, and the denominators.** Four forms are scanned independently over the same words: `jal`
(exact J-target), `lui` + a finishing word within 16 bytes (`addiu`, `ori`, or a load/store whose
offset is 0 through `$at`), any raw 32-bit word equal to the target, and `jalr`. The corpus is the
148,992-word boot `.text` plus **each of the 21 `LEVEL*.BIN` modules at `0x800D12C0`, plus
`BITS/MEMORY.BIN` and `FMV/FMV.BIN` at `0x800D5D20`, one at a time** - the shared slot holds
alternative contents, so the two are never co-resident and a merged image would be a fabrication.
Tooling is `scratch/load_census/corpus.py` (gitignored, one fixed activity directory).

### 2.1 The file loader `0x80082508`: 13 sites, all in the boot image

Paths are resolved from the strings in the image; `dest` from the `lui`/`addiu` at the quoted
instruction. `overlay_map.py` already produced this table; this census re-derives the site list
independently and the two agree on all 13.

| id | site | path | dest | enclosing function and its callers | what it is |
|---|---|---|---|---|---|
| L01 | `0x8003DB50` | `"bits\memory.bin"` (`0x80021E08`) | `0x800D5D20` (`0x8003DB48/4C`) | `0x8003D88C`, called from `0x8004127C`, `0x80041620`, `0x800416B0`, `0x8004176C`, `0x8004185C`, `0x8007BCB4`, `0x8007BF10` (7 callers, measured) | MEMORY overlay; gated on `[0x800A16A8]` != 0 and != 0x10 (`0x8003DB30-0x8003DB38`) |
| L02 | `0x8003DEAC` | `$a0` | **`0x800D12C0`** (`0x8003DEA4/8`) | `0x8003DE9C`, 1 caller `0x8003DCCC` inside `0x8003D88C` | the LEVEL overlay |
| L03 | `0x8003DD9C` | built by `0x80082790` from `"level3.dat"` (`0x80021E60`) or `"level.dat"` (`0x80021E6C`) | `[0x800A1604]`, which `0x8003DCF0-0x8003DCF8` sets to `0x800D5D28` | inside `0x8003D88C` | the level `.dat` |
| L04 | `0x8003EEAC` | `"fmv\fmv.bin"` (`0x80021E78`) | `0x800D5D20` (`0x8003EEA4/8`) | `0x8003EE4C` (`kMemoryStatus`), the intro-movie transaction | **the FMV overlay** |
| L05 | `0x8003FD00` | `"bits\memory.bin"` | `0x800D5D20` (`0x8003FCF4/8`) | `0x8003FCE8`, 1 caller **`0x8007AAB4`** - inside `GameMain`, between `0x8007AA5c jal 0x8003a780` and `0x8007AAB4` | MEMORY overlay, the cold front-end load |
| L06 | `0x800417D0` | `"fmv\fmv.bin"` | `0x800D5D20` (`0x800417C8/CC`) | `0x800416B0`'s callee chain; immediately followed by `0x800417D8 jal 0x800D6628` | **the FMV overlay, second site** |
| L07 | `0x80039230` | `sprintf(0x800ACD8, arg)` - buffer, zero in the pre-boot image | `*($s0)` | `0x800391F4`, callers `0x80041B40`, `0x80041D98` | generic in-level asset |
| L08 | `0x8003B5BC` | `$s0` | `0x801C0000 - 0x42D8 - $v0` (`0x8003B5B0-0x8003B5B8`), a **descending arena above the render buffer at `0x801BBD28`** | `0x8003B544`, callers `0x8003DC2C` (inside `0x8003D88C`) and `0x8003FCA0` | the level asset, `"level{1..3}.raw"` (`0x80021E18/24/30/3C`) |
| L09 | `0x80041AA8` | `sprintf(0x800ACD3C, arg)` | `*($s2)` | `0x80041A10`, callers `0x8003CED0`, `0x800424B4` | generic in-level asset |
| L10 | `0x8005CF28` | `sprintf(0x800ACD44, arg)` | `*($s0)` | `0x8005CED4`, caller `0x8005D53C` | generic |
| L11 | `0x80079BB8` | `"PAD\PATH00.BIN"` (built digit-by-digit, `0x80079B8C-0x80079BAC`) | `0x800BA0C8` (`0x80079BB0/4`) | `0x80079B58`, caller **`0x8007AD14`**, in the boot path | the PAD path table |
| L12 | `0x8007F194` | `$sp+0x10`, `sprintf("<drive>:<name>.vh")` (`0x8007F144-0x8007F150`, `0x800A1024`) | `0x800BB8D8` (`0x8007F18C`) | `0x8007F108` (`LoadVB`), callers `0x8003A810`, `0x8003DA00`, `0x8003DA24`, `0x8003EE80`, `0x800417B4`, `0x8007F300` | the vertex buffer |
| L13 | `0x8007F1CC` | `<drive>:<name>.vb` (`0x800A1028`) | `$s0 + 0x800D5D28` (`0x8007F1C0/4/8`), i.e. **into the shared slot** | same function | the vertex buffer, second half |

`0x80082728` has exactly **1** caller (`0x800825D8`) and `0x80082608` exactly **1** (`0x80082750`), so
the whole file-load surface is the 13 sites above and nothing reaches `0x80082608` another way.

### 2.2 The transactions that call those sites

Three functions compose the 13 sites into the game's actual load phases. All three are **blocking to
their caller**: none contains a `jal 0x80088628` (the linked VSync) or a draw between two loads.

* **`0x8003EE4C` (`kMemoryStatus`) - the intro-movie transaction.** `0x8003EE60 jal 0x8003A218`;
  `0x8003EE80 jal 0x8007F108("sfx\global")` when `$gp+0x538 != 0` (L12+L13, and `0x8003EE88 sh $zero,0x538($gp)`);
  `0x8003EE94 jal 0x8007C278` when `$a1 != 0`; **`0x8003EEAC jal 0x80082508("fmv\fmv.bin",0x800D5D20)` (L04)**;
  `0x8003EEBC jal 0x8007C344` when `$a1 != 0`; `0x8003EEC4 jal 0x800D6628(index)`; `0x8003EECC jal 0x8003A650`.
  The four movie steps are `GameMain`'s own `&&` chain: `0x8007AAF8 jal 0x8007BC74(10,0)` then
  `0x8007AB10 jal 0x8003EE4C(2,0)` and its successors.
* **`0x8003D88C(level)` - the level asset transaction.** L01 (MEMORY overlay, gated) -> L02 (LEVEL
  overlay, via `0x8003DCCC`) -> `0x8003DC2C jal 0x8003B544` (L08, `level{n}.raw`) -> L03
  (`level{n}.dat` -> `0x800D5D28`) -> `0x8003DCD4 jal 0x8003EC30` post-processing. Its 7 callers are
  listed at L01; `0x8007BCB4` and `0x8007BF10` are the front end, the other five are in-level.
* **`0x8007BEC4(level, playbackMode)` - the whole level-entry transaction.** `0x8007BEF0 jal 0x8003A218`;
  `0x8007BF08 jal 0x8007C278(kLevelId, kPlaybackMode)`; `0x8007BF10 jal 0x8003D88C(level)`;
  `0x8007BF28 jal 0x8007C344(...)`; `0x8007BF30 jal 0x8007C5F8`. **One guest call, four blocking
  loads, nothing presented.** `0x8007C278` is called from exactly two places (`0x8003EE94`,
  `0x8007BF08`) and `0x8007BEC4` from exactly one (`0x8007AE14`).

### 2.3 What is on screen during each of those

`0x8003EE4C` and `0x8007BEC4` present nothing between their loads. The only presentation in the whole
level-entry path is inside `0x8007C344`, which is the **fourth** step, i.e. **after** every load has
already finished.

### 2.4 `CdRead 0x80093AF0` / `CdReadySync 0x80093BF4`: 7 sites each, 5 of them in FMV.BIN

Boot image: `0x800826BC`/`0x800826D4` (the file loader) and `0x80093444`/`0x80093450` (libcd's own
`0x80093408`). `FMV.BIN`: `0x800D879C`, `0x800D8864`, `0x800D8C04`, `0x800D8D18`, `0x800D91C4` and the
matching five `CdReadySync`. **No other module issues a CD read** - the 21 `LEVEL*.BIN` and
`BITS/MEMORY.BIN` contribute **0** each.

### 2.5 Closure, and the instrument shown the other answer

`lui`-materialised references: **0** for every loader and every libcd entry point above. Raw 32-bit
words equal to any target: **0**. `jalr` in the corpus: printed image-wide by the same tool; the title
reaches its routines through `jal` (the one dispatch table it uses, FMV's 29-entry `jr $v0` at
`0x800D668C`, indexes **data**, not code, so it cannot reach a loader).

**The same scan, run against two targets it must find, does find them**, which is the only reason the
zeros mean anything:

```
=== target 0x800D5D20 ===   [boot .text] scanned 148992 words; jal 0; materialised 4; raw 0
    mat 0x8003DB48/0x8003DB4C    mat 0x8003EEA4/0x8003EEA8
    mat 0x8003FCF4/0x8003FCF8    mat 0x800417C8/0x800417CC
=== target 0x800D12C0 ===   [boot .text] scanned 148992 words; jal 0; materialised 3; raw 0
    mat 0x8003DEA4/0x8003DEA8    mat 0x80082D68/0x80082D6C    mat 0x80082DB0/0x80082DB4
```

Four materialisations of the shared slot and three of the level slot, against zero for every loader in
the same 148,992 words with the same code.

## 3. Classification

**LOADING-ONLY** means removing the wait removes no authored content. **AUTHORED** means a designed
sequence that stays.

| # | operation | blocking / streaming | what is presented during the wait | class | retail cancel | state in the port |
|---|---|---|---|---|---|---|
| 1 | `CdReadySync(0)` spin, `0x800826D4` | blocking | nothing (no `jal 0x80088628`, no draw, in `[0x80082608,0x80082724)`) | LOADING-ONLY | none (no pad read) | the retail body runs; measured 0-11 ms per whole-file load (section 5) |
| 2 | `CdSearchFile` retry, `0x80082648` | blocking, **unbounded** | nothing | LOADING-ONLY + a hang when the file is absent | none | **the hang of issue 0028** |
| 3 | whole-read retry, `0x80082750`/`0x8008276C` | blocking, **unbounded** | nothing | LOADING-ONLY + a hang | none | reachable today; no bound |
| 4 | L01/L05 `bits\memory.bin` -> `0x800D5D20`, 63,312 B | blocking | nothing | LOADING-ONLY | none | `OverlayImages` authenticates and publishes, then **calls the original** (`game/overlay/overlay_images.cpp:62`) |
| 5 | L02 level overlay -> `0x800D12C0`, up to 19,040 B | blocking | nothing | LOADING-ONLY | none | same observer, `kFileLoader` override at `0x80082508` |
| 6 | L03 `level{n}.dat` -> `0x800D5D28`, up to ~437 KB | blocking | nothing | LOADING-ONLY | none | no owner |
| 7 | L08 `level{n}.raw` -> descending arena, up to ~469 KB, **then `DecompressRAW 0x80021190`** | blocking read; the decode is CPU | nothing | read LOADING-ONLY; the decode is a bounded CPU wait, not I/O | none | finite-slice owner: 28 slices cold, 60 entering Level 1 |
| 8 | L04/L06 `fmv\fmv.bin` -> `0x800D5D20`, 510,960 B, **five times in one route** | blocking | nothing | LOADING-ONLY, and the largest single wait in the game | none | `SharedSlotImage` authenticates and publishes each time |
| 9 | L12/L13 `sfx\global` `.vh`/`.vb`, 3,616 + 426,768 B | blocking | nothing | LOADING-ONLY | none | no owner. **This is also the `0x8007F174` unbounded VSync spin** (see row 12) |
| 10 | L11 `PAD\PATH00.BIN` -> `0x800BA0C8`, boot | blocking | nothing | LOADING-ONLY | none | no owner |
| 11 | L07/L09/L10 generic in-level assets | blocking | whatever the guest last drew | LOADING-ONLY | none | no owner; the measured run shows **six further reads during gameplay** (section 5.3), so this title streams while it plays |
| 12 | `0x8007F174` `VSync(0)` spin behind a `break`, entered when `FileSize & 0xFFFFF800 >= 0x4001` (`0x8007F15C-0x8007F16C`, `$s0` is then set to 0 at `0x8007F170` and never written again) | blocking, **unbounded, calls the linked VSync** | the last frame, frozen | LOADING-ONLY, and an assertion the guest can lose | none | unreachable in this port only because no `.vh`/`.vb` reaches 16 KiB on this route; **not a property of the code** |
| 13 | FMV.BIN's five `CdRead`s (section 1.3) | streaming, one chunk per movie frame | the movie | AUTHORED (it *is* the movie's data path) | none at the read | the framework CD model; `0x800D91D0` is an unbounded spin |
| 14 | `0x8007C344` - the 28-field pre-resident fade, the graphics init, and the **"LEVEL 1: ANDY'S HOUSE, PRESS X" card** | field-paced (`0x8007C3D8 jal 0x8003FA68(1)`), not blocking | the card | AUTHORED | **yes, Cross**: `0x8007C43C-0x8007C440` read the mask, `0x8007C448 andi $v0,$v0,0x4000`, edge-detected against `[0x800A11E4]` at `0x8007C454/60`, then `0x8007C47C jal 0x80077598(0,0,0,0xC)` = `BeginFade`. **Select** (`0x8007C4C0/CC`, mask 1) also acts | transcribed field-by-field by `ResidentPreparation`; the Cross route is what the route's `cross@790` exercises |
| 15 | `0x8007C278` and `0x8007BF10` before `0x8007C344` in `0x8007BEC4` | blocking | nothing | LOADING-ONLY | none | `loadLevelStep` in `game/loop/resident_preparation.cpp` |
| 16 | the four intro movies and the Level 1 intro, `0x800D7088`/`0x800D6628` | one display field per movie frame (`0x800D7590 jal 0x80088628` per issue 0027) | the movie | AUTHORED | **yes**: FMV.BIN `0x800D76EC-0x800D76F4` **Start (8) skips unconditionally**; `0x800D76FC andi 0x4000` **Cross** and `0x800D772C andi 0xF000` any face button skip only when `[0x800A1670] != 0` (`0x800D7720 lh $v0,0x1670($v0)`). Latched by `$s1 = 1` at `0x800D7700`/`0x800D7738`, tested at `0x800D76E0` | guest-owned and reachable; not exercised by any gate (section 9) |
| 17 | boot logos | **there are none.** `GameMain 0x8007A9E8` goes `0x8007AAF8 jal 0x8007BC74(10,0)` -> the four movies -> `0x8007AB58`-onward; no logo state, no logo timer, no publisher-logo frame appears anywhere in the route's 205-field front-end poll, whose only content is the title/menu UI | - | - | - | - |

**Row 17 is a real result, not an absence of looking.** The claim "TS2 has boot logos" is what one
assumes from the era; the census says the first thing the player sees is either the four STRs or the
MEMORY.BIN front end, and the 41 `pad`-read functions plus 9 `Start` reads in MEMORY.BIN belong to the
title/menu/level-select UI. **Nothing in this title waits to be skipped before it starts.**

## 4. The retail cancellation routes (cited)

* **Movies: Start / Cross / face buttons** - FMV.BIN `0x800D76EC` (`lhu $v1,0x1480($v1)` from
  `lui $v1,0x800a`), `0x800D76F4 andi $v0,$v1,8`, `0x800D76FC andi $v0,$v1,0x4000`,
  `0x800D772C andi $v0,$v1,0xF000`, gates `0x800D76E0 bnez $s1` and `0x800D7728 beqz $v0`. This is the
  **only** pad-mask site in the whole 127,740-word module.
* **Level-1 card: Cross, and Select** - `0x8007C448`, `0x8007C460`, `0x8007C4CC`, inside `0x8007C344`.
* **The mask word is `0x800A1480`**, written once per field by `0x8003B33C` (`0x8003B344 lhu $v0,0x1480($v0)`
  -> `0x8003B350 sh $v0,0x11e4($at)` copies it to previous -> `0x8003B354 jal 0x8003AC58` decodes ->
  `0x8003B360 sh $v0,0x1480($at)`); its 2 callers are `0x8003A958` and `0x800D7404` (MEMORY.BIN).
* **Every loader has no route at all.** 28 `lhu` sites of `[0x800A1480]` in the boot image resolve to
  **10** enclosing functions - `0x800742C4` (7), `0x8007B254` (5), `0x8007C344` (4), `0x80074498` (3),
  `0x80074F80` (3), `0x80063AC8` (2), `0x8003B33C` (1, the poller itself), `0x800412F0` (1),
  `0x800734DC` (1), `0x8007B850` (1) - and none of the ten is `0x80082508`, `0x80082728`, `0x80082608`,
  `0x8003D88C`, `0x8003DE9C`, `0x8007C278`, `0x8007BEC4`, `0x8003EE4C`, `0x8007F108` or `0x8003A218`.
  FMV.BIN: 1 site. MEMORY.BIN: 94 sites, all UI. LEVEL.BIN: **0**.
  Denominator: 148,992 + 127,740 + 15,828 + 3,467 words.

## 5. Measured

One headless, silent, unpaced route, `tools/verify_route.py --route`, psxport at the pin `d51ee05a`,
`-G Ninja` + `clang++`, `RelWithDebInfo`. **PASS**: Buzz at pad frame 900, `x=194774 y=60026
z=-361401`; ledger `calls=46107 translated_blocks=8778 executed_instructions=1963419994 faults=0`,
fallback 1,267 instructions, all `load_delay_hazard`. No product code was added.

A second, instrumented run of the same route with `PSXPORT_DEBUG=cd` supplies the `[cd]` lines. All
figures below are read off those two logs (`scratch/load_census/route.log`, `scratch/load_census/cdrun.log`).

### 5.1 Totals

| quantity | value |
|---|---|
| `CdSearchFile` issued | **32** |
| `CdRead` issued | **37** |
| sectors read | **2,652** = **5,431,296 bytes** |
| `FMV/FMV.BIN` overlay reads (510,960 B each) | **5** |
| `BITS/MEMORY.BIN` overlay reads (63,312 B each) | **4** |
| `LEVEL*.BIN` overlay reads | 2 (624 B at selection, 13,868 B entering Level 1) |
| reads issued **during gameplay** (after the resident update began) | 6, totalling 788 sectors = 1,613,824 B |
| whole-file blocking windows, summed (search -> the next non-`[cd]` line) | **764 ms** of a 17 s run |
| of which the five FMV overlay loads | **53 ms** |

### 5.2 Fields and cycles per phase, from the existing log lines

| phase | owner call | fields | slices / turns | cycles | wall |
|---|---|---|---|---|---|
| graphics initialisation | finite guest call | 0 | 2 slices | 792,614 | ~29 ms incl. the AUDIO search |
| cold front-end asset load (`0x8007BC74`) | finite guest call | 0 | 28 slices | **15,245,664** | 161 ms |
| FMV overlay load #1 | inside `kMemoryStatus` | 0 | - | - | 11 ms |
| intro movie 1 | resumable guest call | **194** | 1,555 turns | - | 4,463 ms |
| FMV overlay load #2 -> movie 2 | | 0 / **41** | - / 1,256 turns | - | 11 ms / 2,108 ms |
| FMV overlay load #3 -> movie 3 | | 0 / **73** | - / 1,115 turns | - | 11 ms / 2,162 ms |
| FMV overlay load #4 -> movie 4 | | 0 / **69** | - / 1,314 turns | - | 10 ms / 2,437 ms |
| front-end poll (title/menu attract) | resumable guest call | **205** | 232 turns | - | 198 ms |
| interactive selection | resumable guest call | **62** | 84 turns | - | 401 ms |
| Level 1 intro movie | resumable guest call | **69** | 1,319 turns | - | 2,412 ms |
| level overlay load (`0x8007C278`) | finite guest call | 0 | 9 slices | **5,013,274** | 252 ms incl. `.vh`/`.vb` |
| level asset load (`0x8003D88C`) | finite guest call | 0 | 60 slices | **33,309,042** | 67 ms after the `.dat` read |
| resident update | finite guest call | 2 fields/iteration | 2 slices | 670,594 | - |

**Every load-bound wait in the route costs zero display fields**, because each one runs inside a single
host step: the frame counter does not advance while `0x800826D4`, `0x80082750`, `0x80082508` or
`0x8007C278` are on the stack. The loading-only cost is wall-clock and CPU, not frames - which is why
"remove the loading-only *presentation*" has nothing to remove here, and why the real target is the
blocking **latency** and the two unbounded loops.

### 5.3 Per-file cost, and the title streams while it plays

`search -> CdRead` for each of the 32 searches (this is the issue half only; the completion half is in
the 764 ms above):

```
/SFX/GLOBAL.VH;1            3,616 B      0-1 ms      /FMV/FMV.BIN;1        510,960 B      8 ms  (x5)
/SFX/GLOBAL.VB;1          426,768 B     18 ms        /BITS/MEMORY.BIN;1     63,312 B      2 ms  (x4)
/BITS/MEMORY.BIN;1         63,312 B      2 ms        /LEVEL00/LEVEL.RAW;1  160,484 B      5 ms  (x2)
/LEVEL00/LEVEL.RAW;1      160,484 B      5 ms        /LEVEL06/LEVEL1.BIN;1     624 B      0 ms
/FMV/FMV.BIN;1            510,960 B      8 ms        /LEVEL01/LEVEL.BIN;1  13,868 B      0 ms
/LEVEL06/LEVEL1.RAW;1     165,044 B      9 ms        /LEVEL01/LEVEL.DAT;1 437,164 B     16 ms
/TOY2FMV/AUDIO{1,2,3}.XA;1  ~59 MB       2-4 ms      /GFX/LEVEL1A.RAW;1     81,564 B      4 ms  (x2)
/LEVEL01/LEVEL01.VH;1       5,664 B      0-1 ms      /LEVEL01/LEVEL01.VB;1 509,536 B     22 ms
/LEVEL01/LEVEL.RAW;1      469,276 B     19-20 ms     /LEVEL06/LEVEL1.DAT;1 109,876 B      3 ms
```

The route's six **in-gameplay** reads, in order: `LEVEL01/LEVEL01.VB` 509,536 B, `LEVEL01/LEVEL.RAW`
469,276 B, `LEVEL01/LEVEL.BIN` 13,868 B, `LEVEL01/LEVEL.DAT` 437,164 B, then `CdGetSector 8 words` on
LBA 2236 - the six are the L12/L13/L02/L03/L08 sites again, reached from `0x8003D88C` on the way in,
not during play. **[I]**: a route that stays in Andy's Room for 400 further pad frames issues **no**
further reads (the log has none after `11:59:32.014`), so this route does not yet demonstrate
mid-level streaming. A level with more than one room, or a level transition, would.

### 5.4 What this measurement cannot settle

* The window "search -> next non-`[cd]` line" **includes** the RAW decode that follows a read, so
  161 ms for `LEVEL00/LEVEL.RAW` is read + 15.2 M cycles of decode, not read alone. The read half is
  the 5 ms above.
* Wall-clock is not a per-field cost: the run is unpaced, so a field is as long as the work in it.
  The four movies' 194/41/73/69 fields at 23/51/30/35 ms per field are the honest measure of
  authored movie frames.

## 6. Removal designs

Rules applied: complete owned transitions only; no fast-forwarding; no writing a phase, timer or scene
word; loads asynchronous; authored cutscenes stay; a cancellation route must be recovered or be a
purpose-built skip that reaches the same lifecycle.

**R1. The whole-file blocking wait (rows 1, 4-11). Owner: a new `ts2::cd::FileTransfer`
(`game/cd/file_transfer.{h,cpp}`), installed as a native override of `0x80082608`.** The existing
`OverlayImages` already does every ingredient - `readDiscImage`, `authenticateSource`, SHA-256 of the
source and of the transferred range, write through the canonical memory writer so
translated-block invalidation applies, `OverlaySlot::publish` - and then deliberately calls the
original at `game/overlay/overlay_images.cpp:62`. `FileTransfer` is that body lifted out of the
*overlay* concern (it must serve `.raw`, `.dat`, `.vh`, `.vb` and `PATH00.BIN` too, not just the two
slots), issuing nothing on the CD and returning the exact CdlFILE size so `0x80082608`'s own epilogue
and `0x80082508`'s `+1` are unchanged. Nothing is written to a guest word. Expected effect, from
section 5.3: removes **53 ms** of FMV overlay reads, 8 ms of MEMORY, 16-22 ms of the big `.dat`/`.vb`
reads - about **120 ms** of the 764 ms - at the cost of no behavioural change at all, because a copy
from an authenticated image and a copy from the drive produce the same bytes.

**R2. The two unbounded loops (rows 2, 3). Owner: the same `FileTransfer`.** Retail's
`0x80082750`/`0x8008276C` loop and `0x80082648`'s search loop are unbounded by construction. Under
R1 the file is read from an authenticated source, so the only way to reach those loops is a source
that cannot supply the file - at which point the correct behaviour is a **typed refusal with the path
and the requested size**, not a spin. This is not a behavioural change on any path the disc can
supply; it is the removal of a hang. The refusal must be reachable by a test, or it is decoration.

**R3. The `0x8007F174` VSync spin (row 12). Owner: `FileTransfer` again, through `0x80082008`.**
The loop is entered because `FileSize(name) & 0xFFFFF800 >= 0x4001`. Once the file's length comes from
the authenticated source instead of a CD round trip, the size check is answerable directly and the
guest never enters the loop. This removes a loop that **calls the linked VSync from inside a load**,
which is the most dangerous shape in this title.

**R4. The five `FMV/FMV.BIN` reloads (row 8).** No separate design. Under R1 each reload is a copy
from an image already authenticated in this run, and the *load itself* still happens - which is what
the rule requires. What is removed is the storage wait, not the load. **Do not** add "skip the reload
if the identity is active": that is a fast-forward of a transaction the guest still performs.

**R5. `0x8007C344` and the movies (rows 14, 16): nothing to remove.** Both are authored, both are
field-paced already, and both have their retail cancel in guest code that the port already runs. The
only work is **proof**: two runs with a forced Start edge at a named movie field and a named card
field, showing the movie returns earlier and the card's `BeginFade` fires, plus the negative (no press
-> no change). Until that exists, `docs/project-state.md` must not claim the logos/cancel route is
satisfied - and note there are **no logos** (row 17).

**R6. Loading presentation: there is none, and that must be stated with a denominator.** The test
belongs in `tools/verify_route.py`: over the route, assert that no presented frame is delivered while
the guest is inside `0x80082608`, `0x80082728`, `0x80082508` or `0x8007C278`, and print the count of
such frames (expected **0 of 1000**) next to the count of frames those functions were on the stack
(expected non-zero). A pass that prints "0" without the second number is a dead tap.

**R7. Debt the census exposes, which is a prerequisite, not a side note.** `prepareResident`
(`game/loop/toystory2_frame_driver.cpp`) writes **11** guest words and `ResidentPreparation::finish`
(`game/loop/resident_preparation.cpp`) writes **about 40** more - including `kElapsedFields`,
`kExitCountdown = 90`, `kTransitionFlags | 1`, `[0x800A1370] = 900`, `[0x800A1464] = 0xCE` and two
`0xFFFF8000` stores. These are phase, timer and scene words written by the port, which is exactly what
the loading rule forbids, and they are also what makes the level-entry transition unprovable against
retail today. R1-R3 can land independently; **the ledger that makes them provable cannot** until those
writes are either re-derived from guest state or removed. Name it in the goal's success conditions
rather than discovering it at the end.

**New class?** One: `FileTransfer`. No second owner is needed, and splitting the overlay
authentication out of `OverlayImages` is the only structural move this census implies.

## 7. What would prove payload and terminal state identical to retail

* **M1 payload.** Per `LoadFile` call, compare the multiset of `(issuer site, guest path, destination,
  byte length, SHA-256)` between a retail-paced oracle run and an R1 run. Denominator: the 13 sites of
  section 2.1, and which were **not** reached by the route (L05, L07, L09, L10, L11 and `0x8003B544`'s
  second caller were not). Negative: point one module's digest at a truncated file and require M1 to
  report the operation unequal - `OverlaySlot::authenticateSource` already has that seam.
* **M2 terminal state.** At each transaction's natural return - `0x80082508`'s size return, `0x8003D88C`'s
  post-`0x8003EC30` state, `0x8007C278`'s return, `0x8003EE4C`'s `0x8003A650` return - compare a
  named field list against retail. **The expected result is not "all equal"**: `prepareResident` and
  `ResidentPreparation::finish` write those fields (R7), so those are the known deltas and must be
  listed as such rather than discovered.
* **M3 residual latency.** With R1-R3 landed, re-run the instrumented route and require the summed
  whole-file windows to fall from 764 ms to the decode time alone. **Shown the other answer**: run
  once with `FileTransfer` disabled and require the same numbers to return.
* **M4 unbounded paths.** A test that makes `FileSize` return `0x5000` for a `.vb` and requires the
  `0x8007F174` spin to be unreachable. Today that case is unreachable on this route, which is why it
  is a test and not a claim.

## 8. Two instrument defects this census hit (both would have published a wrong number)

**8.1 `ori` counted as `andi`.** The first button-mask scan used `(w & 0xFC000000) == 0x34000000`,
which is true for `ori` as well as `andi`. It reported **one** Start read in FMV.BIN, at `0x800D95AC` -
inside an MDEC inverse-quantiser (`ori $t1,$zero,8` between `bltz`/`sll`), and one `0x8000` read at
`0x800D719C` that is an argument `ori $a1,$a1,0x8000` on a `jal` delay slot. With the opcode test
`(w >> 26) == 0x0C` the same scan reports FMV.BIN Start at **`0x800D76F4`** and Cross at
**`0x800D76FC`** - the real skip - and **zero** for `0x800D95AC`. A "loading-only wait" that a
confident scan placed inside an audio decoder is exactly the kind of number that survives review.

**8.2 `lui` + offset-0 load counted as a materialisation.** The first `lui`-pair rule accepted any
load whose offset was 0, which matched `lui $v1,0x8008` followed by `lbu $v0,($v1)` in
`LEVEL10__LEVEL1.BIN` and reported **five** false hits against `0x80082508`, `0x80082728` **and**
`0x80082608` in the same module. Requiring the `lui` to target `$at` removed all five. The
fixed instrument reports **0** module-side references for all three, which is why section 2 quotes the
`0x800D5D20`/`0x800D12C0` positive control next to the zero.

## 9. Verified versus inferred

**Byte-verified in this session [B]:** every address and word quoted above; the three-function shape of
the file loader and both spin loops; the 13 issuer sites and their path/dest arguments; the 1/1 caller
counts for `0x80082728` and `0x80082608`; the three composing transactions and their call order; the
7 `CdRead` and 7 `CdReadySync` sites and the 5 in FMV.BIN; the 29-entry FMV dispatch table; the
closed zero with its positive control; the pad-mask word, its writer, its 2 callers, its 10 boot-image
functions and the 1/94/0 module counts; the FMV skip masks and the `[0x800A1670]` gate; the level-1
card's Cross and Select; the `0x8007F174` spin and the condition that reaches it; and every number in
section 5, which is read off two run logs.

**Read from source only [S]:** what `OverlayImages` authenticates and where it calls the original; the
finite-slice and `ResidentPreparation` structure; the guest words the port writes (R7).

**Inferred [I], each with what settles it:**

* the argument roles of `0x80090FA4` at `0x80082674` - its first argument is `&loc`, which does not fit
  the four-argument sender's `u8 control`; settled by reading `0x80092AE8`'s writer of that buffer
  rather than its prologue;
* that `0x80082008` returns a size (from `0x8007F15C`'s use, not from its body);
* that `0x800893B4` - reached only from FMV.BIN's 5 sites, with **0** `jal` callers in the boot image
  and a body region that decodes as a 12-byte-entry table rather than code - is a per-track XA setup;
  it is **not** counted as a load primitive anywhere above;
* that the six late reads in section 5.3 are all level-entry rather than in-play, from the absence of
  further reads; settled by a longer route that crosses a room boundary;
* the resident-phase field cost (`displayFieldQuota` returns 2) versus a display field;
* the per-field cost of the movies, from wall clock in an unpaced run.

**Not done:** no oracle comparison (M1, M2); no run with a forced Start or Cross edge (R5); no route
that reaches L05, L07, L09, L10, L11 or a second level; `0x80082E6C`/`0x80082E9C` bodies not
disassembled; `tools/verify_route.py --determinism` and `--negative` not re-run (they are issue
0034's gate and are unaffected by this census).

## 10. Method notes for whoever repeats this

* Build the RAM image with `tools/ram_image.py`, never by mapping the file from its start: the text
  is at file offset `0x800` and lands at `t_addr 0x80010000`.
* **One module per placement.** `MEMORY.BIN` and `FMV.BIN` share `0x800D5D20` and never co-reside;
  a merged image silently clobbers one with the other and every disassembly afterwards is fiction.
* The pad mask is a **word** read with `lhu`, so `lui $r,0x800a` + `lhu 0x1480($r)`. Button masks are
  `andi` (opcode 6) and `ori` (opcode 13) - test the opcode, not the 0x34 prefix.
* A `jal`-only census is safe here **and only because it was closed**: the materialised and raw-word
  forms both return 0 for every loader, with the `0x800D5D20`/`0x800D12C0` control proving the scan
  can fire.
* The first build of this worktree pointed `PSXPORT_DIR` at the shared checkout HEAD `72bd5293` rather
  than the pinned worktree, and the run *failed* with a pad-replay refusal
  ("not a phase-keyed pad recording: the file does not start with the PSXPADPH magic"). It is recorded
  here because a framework off the pin can produce a run that fails for a reason that has nothing to do
  with the change under test, and the obvious reading - "the route is broken" - is wrong.