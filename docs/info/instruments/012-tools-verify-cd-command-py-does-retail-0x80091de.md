---
id: I012
kind: instrument
status: trusted
created: 2026-08-21
---

## Instrument

tools/verify_cd_command.py — does retail 0x80091DE4 command/completion ABI derive, and is a live CDC sector sequence physically drive-paced?

## Validated by

I012: shipping SLUS_008.93 derived command 0x80091DE4, service 0x80091310 and sync 0x80091898; a nop mutation at the pre-command sync jal is named/refused. Synthetic serviced traces produce both answers: 80 1x sectors in one second is classified impossible, while two is bounded; a synthetic `cdcr` INT1 response proves the status parser can return `0x22`. Real 0xA0 live traces also produce both answers with INT acknowledgement and DMA service present: old psxport `692b9b20` classifies 21,164 contiguous sectors as impossible, while both direct and default routes against exact pinned `3418a79b624765614f3f198dc1e89632e1e650f0` classify 358 total sectors across eleven Setloc/ReadN phases, with the longest bounded at 209 sectors. The direct and default `cdcr` traces independently observe and gate INT1 status `0x22`.

## Known failure modes

The live bound uses the configured watchdog timeout plus boot grace as a conservative observation
denominator; it distinguishes an impossible runaway from a bounded run but does not measure exact
per-sector jitter. Traces with multiple Setloc/ReadN phases are segmented before the longest phase is
classified so an early Setloc is never compared to a later sector sequence.
