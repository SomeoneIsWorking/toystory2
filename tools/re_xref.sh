#!/usr/bin/env bash
# re_xref.sh — run tools/ghidra_xref.py over an already-imported Ghidra project.
#
#   tools/re_xref.sh --selftest
#   tools/re_xref.sh scratch/decomp/xref-overlay-names.txt 80022F84..80022FAC
#
# The project is built ONCE by the framework's importer, which is the only supported importer here
# (it bases the flat image at 0x80000000 so every Ghidra address IS a guest address):
#   python3 tools/ram_image.py
#   external/psxport/tools/decomp.sh import scratch/ghidra/ram-boot.bin ts2boot
#
# Ghidra 12 dropped bundled Jython, so `analyzeHeadless` cannot run a .py postScript at all; the
# PyGhidra entry point `pyghidraRun -H` takes the same CLI. Resolved from $PATH on purpose — a
# filesystem path here would be machine-specific and this repo is public.
set -eu
repo="$(cd "$(dirname "$0")/.." && pwd)"
proj="${TS2_GHIDRA_PROJECT:-ts2boot}"
if [ ! -e "$repo/scratch/ghidra/$proj.gpr" ]; then
  echo "re_xref.sh: no Ghidra project scratch/ghidra/$proj.gpr — import it first (see the header of" >&2
  echo "  this script). Running the script over nothing would print a clean report about nothing." >&2
  exit 2
fi
command -v pyghidraRun >/dev/null || {
  echo "re_xref.sh: pyghidraRun is not on \$PATH (Ghidra's support/pyghidraRun). RE-00 is the step" >&2
  echo "  that stands this up; see docs/re-frontier.md." >&2
  exit 2
}
cd "$repo"
mkdir -p scratch/decomp scratch/logs
status="$repo/scratch/logs/ghidra-xref.status"
rm -f "$status"
# Ghidra headless exits 0 no matter what a postScript does, so its exit code says nothing about the
# analysis. The script writes its verdict to $status and this wrapper exits on THAT — an absent file
# means the script never got far enough to have an opinion, which is a failure, not a pass.
pyghidraRun -H scratch/ghidra "$proj" -process -noanalysis \
  -scriptPath "$repo/tools" -postScript ghidra_xref.py "$@" \
  -scriptlog scratch/logs/ghidra-xref.log
if [ ! -f "$status" ]; then
  echo "re_xref.sh: ghidra_xref.py wrote no verdict to scratch/logs/ghidra-xref.status — it did not" >&2
  echo "  run (Ghidra headless still exits 0). Treating as FAILURE." >&2
  exit 2
fi
rc="$(cut -d' ' -f1 "$status")"
[ "$rc" = 0 ] || echo "re_xref.sh: ghidra_xref.py verdict: $(cat "$status")" >&2
exit "$rc"
