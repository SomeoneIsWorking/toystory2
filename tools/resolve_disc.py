#!/usr/bin/env python3
"""Where is this port's disc image? Prints the resolved path on stdout and nothing else (diagnostics on stderr).

Resolution order:

  1. a CLI argument                          python3 tools/resolve_disc.py /path/to/disc.chd
  2. $PSXPORT_TS2_DISC                        the per-game env key, also GameConfig::discEnvVar
  3. .env                                    PSXPORT_TS2_DISC= (or the generic PSXPORT_DISC=)
  4. a *.chd dropped in the repo root        the zero-configuration path

Exits 2 naming the sources tried when none resolves, and also when a source names a path that does not
exist, so a run never silently reads a different disc than the one configured.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ENV_KEY = "PSXPORT_TS2_DISC"
GENERIC_KEY = "PSXPORT_DISC"


def _from_dotenv(path):
    """Return (value, key) from .env, or (None, None). Does not evaluate the file as shell."""
    if not os.path.isfile(path):
        return None, None
    txt = open(path, encoding="utf-8", errors="replace").read()
    for key in (ENV_KEY, GENERIC_KEY):
        m = re.search(r"^[ \t]*" + key + r"[ \t]*=[ \t]*(.+?)[ \t]*$", txt, re.M)
        if m:
            return m.group(1).strip().strip('"').strip("'"), key
        # A path with a literal quote is rejected by the existence check below.
    return None, None


def resolve(argv_path=None, *, verbose=False):
    """Resolve the disc. Returns the path, or raises SystemExit(2) naming what it tried."""
    tried = []

    if argv_path:
        tried.append(("CLI argument", argv_path))
    env = os.environ.get(ENV_KEY) or os.environ.get(GENERIC_KEY)
    if env:
        tried.append((f"${ENV_KEY}", env))
    dot, dotkey = _from_dotenv(os.path.join(ROOT, ".env"))
    if dot:
        tried.append((f".env ({dotkey})", dot))
    drops = sorted(f for f in os.listdir(ROOT) if f.lower().endswith(".chd"))
    for d in drops:
        tried.append(("*.chd in the repo root", os.path.join(ROOT, d)))

    for source, path in tried:
        if os.path.isfile(path):
            if verbose:
                print(f"[disc] {source}: {path}", file=sys.stderr)
            return path
        print(f"[disc] {source} names {path!r} — NO SUCH FILE", file=sys.stderr)
        raise SystemExit(2)

    print(
        "[disc] no disc image. Tried, in order: a CLI argument, "
        f"${ENV_KEY}, .env, and a *.chd in {ROOT}. "
        "Provide the game's own disc image — it is never shipped with this repo.",
        file=sys.stderr,
    )
    raise SystemExit(2)


if __name__ == "__main__":
    print(resolve(sys.argv[1] if len(sys.argv) > 1 else None, verbose=True))
