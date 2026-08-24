---
id: 18
title: sentinel substring check accepts a stream that runs off the end without a sentinel
status: resolved
symptom: a complete valid chunk followed by EOF is reported clean because its stop reason says "without a sentinel"
tags: assets,formats,instrument,raw
created: 2026-08-24
updated: 2026-08-24
---

## Root cause

Both `.RAW` tools tested `"sentinel" in reason` to decide whether the framing walk reached the
`0xFFFFFFFF` terminator. The negative reason `ran off the end without a sentinel` contains the same
word, so an otherwise valid final chunk at exact EOF passed. The old truncation control happened to
stop on `implausible lengths` and therefore could not expose this neighboring failure shape.

## Resolution

`raw_probe.py` now owns the exact `SENTINEL_REASON` value and both tools require equality. The
copyright-free hermetic decoder fixture deliberately removes only the four-byte sentinel; that
negative now exits 1 while the complete fixture exits 0. The entire 46-file / 813-chunk retail corpus
still passes. No decoded bytes or format constants were changed.
