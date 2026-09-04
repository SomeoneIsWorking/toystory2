---
id: 31
title: psxport Lightrec dynarec-only backend is not linked
status: open
symptom: The Toy Story 2 product stops at the first guest call with an explicit executor-unavailable fault
tags: dynarec,lightrec,psxport,execution
state_items: S002,S004,S006,S013
created: 2026-09-04
updated: 2026-09-04
---

The title's static generated-code path has been removed. Its guest calls now enter psxport's typed
runtime execution boundary, which deliberately returns `Lightrec dynarec-only backend is not linked`.

Upstream Lightrec configurations can interpret blocks marked never-compile and may route optimized
impossible branches into the interpreter. That violates the gameplay contract. psxport needs a pinned
maintained fork whose product build replaces those paths with explicit exits/retranslation and whose
link inspection proves interpreter objects and symbols are absent. This title must not add a local
executor or restore a fallback.
