---
id: 17
title: post-LEVEL01 model-pointer slot contains payload halfwords interpreted as 0xEDA4F893
status: investigating
symptom: deeper retail route fails read16 at 0xEDA4F893 through resident model consumer 0x800426E0
tags: render,models,pointers,re16
created: 2026-08-24
updated: 2026-08-24
---

## Classified cause

This is not a missing host hardware mapping. Resident `8002518C` selects a package from the model
table at `0x800C7268`, reads a nested model pointer from `package + 8 + index*4`, and passes it to
`800426E0`; that function immediately dereferences the supplied pointer. The failing value is formed
from coordinate-like signed halfwords in the payload at `0x8013B770`. Loader `80041A10` owns the
package-table write and calls relocator `800418C0`, which normally turns the nested offsets into
absolute addresses.

## Still unresolved

Static evidence does not distinguish four producer failures: a stale table after arena reuse, an
overwritten package, a later load that skipped relocation, or a wrong object/model index. The next
observation must identify the last writer of the selected table/package slots or preserve the final
RAM image and compare it with the earlier valid package. Do not map `0xEDA4F893`, skip the read, or
replace the pointer with an expected address; each would hide the producer that actually diverged.

## Evidence

`scratch/logs/frame-fence-final.log` contains the exact register/byte diagnostic. Reproducible static
decompiles are under `scratch/decomp/re16-{pointer-chain,model-table-producer,model-relocator}.c`.
Claim C021 records the falsifier; RE-16 records the remaining dependency.
