---
id: 31
title: psxport Lightrec dynarec-only backend is not linked
status: resolved
symptom: The Toy Story 2 product stops at the first guest call with an explicit executor-unavailable fault
tags: dynarec,lightrec,psxport,execution
state_items: S002,S004,S006,S013
created: 2026-09-04
updated: 2026-09-08
---

Resolved by reconnecting the title to psxport's maintained Lightrec build. The product and its
three native boundary binaries link, and linked execution-boundary inspection passes. Current
runtime qualification and the distinct platform-initialization exit are recorded once in
S002 of `docs/project-state.md`. No title-local executor was added.
