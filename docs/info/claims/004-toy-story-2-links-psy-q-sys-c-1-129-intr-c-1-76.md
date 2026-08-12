---
id: C004
kind: claim
status: holds
created: 2026-08-12
tags: psyq,lineage,repo-shape
depends: docs/info/exe-identity.txt
---

## Claim

Toy Story 2 links PSY-Q sys.c 1.129 / intr.c 1.76 / bios.c 1.86 — the SAME sys.c as Tomba! 1 but NOT the same library triple, and the same full triple as CRASH2 and SPYRO1. Both of TS2's top similarity cells therefore have an SDK explanation and none has an engine explanation: TS2 is STANDALONE

## Evidence

MEASURED 2026-08-12 by reading the version strings out of the extracted SLUS_008.93 (strings | grep for the RCS Id keyword): $Id: sys.c,v 1.129 1996/12/25 03:36:20 noda Exp $, $Id: intr.c,v 1.76 1997/02/12 12:45:05 makoto Exp $, $Id: bios.c,v 1.86 1997/03/28 07:42:42 makoto Exp $, plus Library Programs (c) 1993-1997 Sony Computer Entertainment Inc. CONFIRMS what WORKSPACE.md records (sys.c 1.129) and REFINES it in a direction that STRENGTHENS the standalone decision rather than threatening it. The recorded argument is that TS2 largest cell is 8.2% with TOMBA1 = 0.7x the same-PSY-Q null maximum of 11.89%, i.e. BELOW what two unrelated studios linking one SDK score; since the metric stratifies on sys.c and TS2 and TOMBA1 share sys.c 1.129, that cell is correctly read against the same-PSY-Q stratum and the argument stands unchanged. THE REFINEMENT: measured over all 19 corpus binaries, the claim that those two link the SAME PSY-Q 1.129 is true only at sys.c granularity — TOMBA1 is sys 1.129 / intr 1.74 / bios 1.81, so the two do NOT link the same libraries. TS2 exact triple is shared by precisely two other binaries in the corpus, CRASH2 and SPYRO1, and CRASH2 is TS2 SECOND-highest cell at 6.2%. So cell #1 shares TS2 sys.c and cell #2 shares TS2 entire triple: both of the only two cells worth explaining have an SDK explanation, and nothing points at shared engine code. Cohort placement: TS2 sits in the 1.129 sys.c cohort with TOMBA1 and SPYRO1, the oldest of the three cohorts present (CRASH1 1.120 is older but a singleton); the workspace large cohort is 1.140/1997-98. WHAT COULD NOT BE MEASURED, and it is the one honest gap in the lineage picture: juanmv94 states all 7 TT PSX games share Dave Dootson engine, incrementally revised. There is no second Traveller's Tales disc in /mnt ROM library, so there was NO second binary to compare and the lineage claim rests on one person README — far weaker than the field-exact struct agreement recorded in docs/references.md. Note that a positive would change NOTHING about repo shape: TS2 is standalone and any sibling title would be a reference, not a title in this tree.

## What would falsify it

another Traveller's Tales PSX binary (Toy Story Racer SLUS_012.14, A Bug's Life, Buzz Lightyear of Star Command, Muppet Race Mania, Rascal, Weakest Link) measuring above 3x the recorded cross-studio null maximum against SLUS_008.93 on BOTH psxport/tools/exe_similarity.py and the independent tools/lineage_probe.py — that would establish a real engine lineage and would justify revisiting repo shape. A cell below the null max, or one tool alone, would not. Also falsified if the version strings are re-read and differ, or if the workspace null distribution is recalibrated again
