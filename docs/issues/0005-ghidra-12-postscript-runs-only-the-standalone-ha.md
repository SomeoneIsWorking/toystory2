---
id: 5
title: Ghidra 12 postscript runs only the standalone half of ghidra_xref.py
status: resolved
symptom: python3 tools/re_xref.py --selftest refuses because no verdict appears although Ghidra logs fold checks as passing
tags: instrument,ghidra,pyghidra,workflow
created: 2026-08-20
updated: 2026-08-20
---

The Ghidra xref wrapper removes its stale status, launches the postscript, then finds no new verdict. The log shows only the pure-Python fold; Ghidra's Reference DB controls never run. Investigate the postscript/standalone dispatch before trusting any prior xref output.

### Resolution (2026-08-20)
Root cause: PyGhidra 3 / Ghidra 12 exposes getScriptArgs through its injected script namespace but does not put currentProgram in globals(); the old globals() test therefore selected standalone_main inside a real postscript. Fix: detect the API operation the script actually needs (getScriptArgs), keep the independent status-file verdict, and replace the shell wrapper with tools/re_xref.py. Reproduced on a fresh ts2boot_re00 import: 10/10 fold checks and 5/5 cross-method controls pass, including a Ghidra+fold positive at 0x800103EC and three untouched-high-RAM negatives.
