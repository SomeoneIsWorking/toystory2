---
id: 1
title: psxport_smoke cannot be built from a consumer tree (framework defect, carried forward)
status: open
symptom: cmake -DPSXPORT_BUILD_SMOKE=ON fails: cannot find source file tools/smoke/psxport_smoke.cpp
tags: build,framework,known-defect
created: 2026-08-12
updated: 2026-08-12
---

Carried forward from `vagrant` and `megamanx4` — recorded here so nobody re-diagnoses it, NOT
re-fixed here.

`external/psxport/cmake/psxport.cmake` names `tools/smoke/psxport_smoke.cpp` RELATIVELY, so with
`-DPSXPORT_BUILD_SMOKE=ON` CMake looks for it under THIS repo and fails. The framework's own
game-agnosticism proof is therefore only runnable from inside the framework repo.

* Consequence for this tree: leave `PSXPORT_BUILD_SMOKE` at its OFF default. The gate that runs here is
  `--target toystory2_seam`.
* The fix is one line upstream (`${PSXPORT_ROOT}/…`) and a game repo may not make it — framework edits
  happen in the workspace dev clone only. Hand it to the operator.
