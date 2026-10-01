#!/usr/bin/env python3
"""execution_ledger.py — read psxport's guest-execution ledger back from a log or the control channel.

The ledger is five `name: key=value ...` groups with one owner on the framework side
(runtime/cpu/execution_ledger.cpp): `guest`, `invalidations_by_source`, `invalidation_work`,
`budget_exit`, `fallback`. A default run prints them at run end as `[guest] run-end: <group>: ...` and a
live run answers the same text to the `guest` control command. A reply missing any group is a refusal
(an older binary, or a report that did not run), never a set of zeros.
"""

from __future__ import annotations

import re
import sys

GROUPS = ("guest", "invalidations_by_source", "invalidation_work", "budget_exit", "fallback")
FIELD = re.compile(r"(\w+)=(\d+)")
RUN_END = "[guest] run-end: "

Ledger = dict[str, dict[str, int]]


def parse(text: str) -> Ledger:
    """The ledger groups found in `text` (log lines or a control reply); the LAST of each group wins."""
    found: Ledger = {}
    for line in text.splitlines():
        if RUN_END in line:
            line = line.split(RUN_END, 1)[1]
        name, separator, rest = line.partition(":")
        if separator and name in GROUPS:
            found[name] = {key: int(value) for key, value in FIELD.findall(rest)}
    missing = [group for group in GROUPS if group not in found]
    if missing:
        raise ValueError(f"the text carries no ledger group(s) {missing}; not measured")
    return found


def render(ledger: Ledger) -> list[str]:
    return [f"{group}: " + " ".join(f"{key}={value}" for key, value in ledger[group].items()) for group in GROUPS]


SAMPLE = (
    "[t] [guest] run-end: guest: calls=2 translated_blocks=5 executed_blocks=7 executed_instructions=11 "
    "host_dispatches=1 cache_hits=3 cache_misses=5 memory_callbacks=13 invalidations=17 faults=0\n"
    "[t] [guest] run-end: invalidations_by_source: cpu=15 mapped_store=1 dma=1 module_load=0 debugger=0 "
    "savestate=0 native=0\n"
    "[t] [guest] run-end: invalidation_work: lightrec_calls=17 words_examined=19 guarded_calls=12 walks=5 "
    "block_scans=23 revoked_blocks=2\n"
    "[t] [guest] run-end: budget_exit: exits=1 pc_in_code_image=1 pc_outside_code_image=0\n"
    "[t] [guest] run-end: fallback: calls=0 instructions=0 refused_calls=0 compilation_failed=0 "
    "self_modifying_code=0 unsupported_block=0 load_delay_hazard=0 unsafe_instruction_fetch=0 "
    "refused_compilation_failed=0 refused_self_modifying_code=0 refused_unsupported_block=0 "
    "refused_load_delay_hazard=0 refused_unsafe_instruction_fetch=0\n"
)


def main() -> int:
    print(__doc__)
    return 0


if __name__ == "__main__":
    sys.exit(main())
