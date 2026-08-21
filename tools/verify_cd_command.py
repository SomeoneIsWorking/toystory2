#!/usr/bin/env python3
"""Verify Toy Story 2's stock-libcd command/completion ABI and classify CDC traces.

The static half reads the verified SLUS_008.93 instruction stream. It proves that ``0x80091DE4``
is the four-argument stock command sender, that ``0x80091310`` owns the hardware interrupt-reason
to libcd-state transition, and that ``0x80091898`` is the sync poll/callback dispatcher. It does not
claim that the game's ``(path, dest)`` loader matches psxport's ``(dest, lba, size)`` seam.

The trace half distinguishes two real answers. A drive-paced ReadN may expose no more sectors than
the selected speed permits during the watchdog observation window. A contiguous sequence beyond
that physical upper bound is a controller-service race, not a stuck command poll. ``--selftest``
checks the shipping executable, falsifies one command-flow instruction, and feeds the classifier
both bounded and impossible traces.
"""

from __future__ import annotations

import argparse
import itertools
import math
import re
import struct
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import verify_crt0

DEFAULT_EXE = ROOT / "scratch" / "bin" / "toystory2" / "SLUS_008.93"

CD_COMMAND = 0x80091DE4
CD_SERVICE = 0x80091310
CD_SYNC = 0x80091898
CD_GET_SECTOR = 0x80091108


class Refused(Exception):
    """The supplied bytes or trace cannot support the requested conclusion."""


def jal_target(pc: int, word: int) -> int:
    if word >> 26 != 3:
        raise Refused(f"0x{pc:08X} is not jal (word 0x{word:08X})")
    return ((pc + 4) & 0xF0000000) | ((word & 0x03FFFFFF) << 2)


def absolute_address(exe, upper_pc: int, lower_pc: int) -> int:
    upper = exe.word(upper_pc)
    lower = exe.word(lower_pc)
    if upper >> 26 != 0x0F:
        raise Refused(f"0x{upper_pc:08X} is not lui (word 0x{upper:08X})")
    lower_op = lower >> 26
    if lower_op not in (0x08, 0x09, 0x0D, 0x23, 0x28):
        raise Refused(
            f"0x{lower_pc:08X} does not finish an absolute address (word 0x{lower:08X})"
        )
    value = (upper & 0xFFFF) << 16
    immediate = lower & 0xFFFF
    if lower_op in (0x08, 0x09, 0x23, 0x28) and immediate & 0x8000:
        immediate -= 0x10000
    return (value + immediate) & 0xFFFFFFFF


def require_word(exe, pc: int, expected: int, label: str) -> None:
    actual = exe.word(pc)
    if actual != expected:
        raise Refused(
            f"{label} changed at 0x{pc:08X}: got 0x{actual:08X}, expected 0x{expected:08X}"
        )


@dataclass(frozen=True)
class Abi:
    command: int
    service: int
    sync: int
    get_sector: int
    param_count_table: int
    last_position: int
    last_mode: int
    current_command: int
    sync_state: int
    ready_state: int
    sync_callback: int
    ready_callback: int
    sync_result: int
    ready_result: int
    mmio_status_slot: int
    mmio_command_slot: int
    mmio_param_slot: int
    mmio_irq_slot: int
    service_map: tuple[tuple[int, str, str], ...]


def analyze(exe) -> Abi:
    # a0/a1/a2/a3 are preserved as s0/s1/s5/s2. These are semantic register moves, not an inferred
    # C prototype copied from a decompiler.
    for pc, word, label in (
        (0x80091DF4, 0x00A08821, "param pointer a1 -> s1"),
        (0x80091DFC, 0x00C0A821, "result pointer a2 -> s5"),
        (0x80091E04, 0x00E09021, "async flag a3 -> s2"),
        (0x80091E0C, 0x00808021, "command a0 -> s0"),
        (0x80091E4C, 0x00021880, "command index scaled by four"),
        (0x80091F70, 0x90A20000, "parameter byte load"),
        (0x80091F78, 0xA0620000, "parameter FIFO byte store"),
        (0x80091FA0, 0xA0500000, "command-register byte store"),
        (0x800920F4, 0x00408021, "service result preserved"),
        (0x80092100, 0x1040000C, "ready callback mask test"),
        (0x8009213C, 0x3C02800A, "sync callback load upper"),
    ):
        require_word(exe, pc, word, label)

    pre_sync = jal_target(0x80091EA8, exe.word(0x80091EA8))
    service = jal_target(0x800920EC, exe.word(0x800920EC))
    sync_service = jal_target(0x800919F4, exe.word(0x800919F4))
    if pre_sync != CD_SYNC:
        raise Refused(
            f"pre-command sync target changed: 0x80091EA8 calls 0x{pre_sync:08X}, not 0x{CD_SYNC:08X}"
        )
    if service != CD_SERVICE or sync_service != CD_SERVICE:
        raise Refused(
            f"command/sync paths disagree on service routine: 0x{service:08X}, 0x{sync_service:08X}"
        )

    # The service switch indexes INT1..INT5 through this retail jump table. Each tuple is
    # (hardware interrupt reason, libcd state byte written, callback-result mask returned).
    jump_table = tuple(exe.word(0x800236EC + index * 4) for index in range(5))
    wanted_handlers = (0x800916C8, 0x8009167C, 0x8009157C, 0x8009174C, 0x800917D0)
    if jump_table != wanted_handlers:
        raise Refused(
            "INT1..INT5 service jump table changed: "
            + ", ".join(f"0x{handler:08X}" for handler in jump_table)
        )

    # Prove each handler's externally visible answer from its stores/return delay slot. INT3 has
    # command-dependent state 2/3/5, recorded as state -1 below.
    for pc, word, label in (
        (0x800916F0, 0xA0220AE5, "INT1 ready-state store"),
        (0x800916CC, 0x24020001, "INT1 normal state"),
        (0x800916E8, 0x24020005, "INT1 error state"),
        (0x8009173C, 0x24020004, "INT1 ready callback mask"),
        (0x8009168C, 0xA0220AE4, "INT2 sync-state store"),
        (0x80091680, 0x24020002, "INT2 normal state"),
        (0x80091684, 0x24020005, "INT2 error state"),
        (0x800916C4, 0x24020002, "INT2 sync callback mask"),
        (0x80091588, 0x24630AE4, "INT3 sync-state address"),
        (0x80091580, 0x24020005, "INT3 error state"),
        (0x800915EC, 0x24020003, "INT3 second-response state"),
        (0x8009163C, 0x24020002, "INT3 complete state"),
        (0x80091630, 0x24020001, "INT3 second-response callback mask"),
        (0x800915C4, 0x24020002, "INT3 sync callback mask"),
        (0x8009175C, 0xA0220AE6, "INT4 end-state store"),
        (0x80091770, 0xA0220AE5, "INT4 ready-state store"),
        (0x800917CC, 0x24020004, "INT4 ready callback mask"),
        (0x800917E4, 0xA0220AE5, "INT5 ready error store"),
        (0x800917F4, 0x24420AE4, "INT5 sync-state address"),
        (0x80091854, 0x24020006, "INT5 both-callback mask"),
    ):
        require_word(exe, pc, word, label)

    sync_state = absolute_address(exe, 0x800918D4, 0x800918D8)
    command_sync_state = absolute_address(exe, 0x80091F04, 0x80091F08)
    if command_sync_state != sync_state:
        raise Refused(
            f"command and sync disagree on completion state: 0x{command_sync_state:08X} vs "
            f"0x{sync_state:08X}"
        )
    require_word(exe, 0x800918DC, 0x26540001, "ready state is sync state + 1")
    return Abi(
        command=CD_COMMAND,
        service=service,
        sync=pre_sync,
        get_sector=CD_GET_SECTOR,
        param_count_table=absolute_address(exe, 0x80091E50, 0x80091E58),
        last_position=absolute_address(exe, 0x80091ECC, 0x80091ED4),
        last_mode=absolute_address(exe, 0x80091EFC, 0x80091F00),
        current_command=absolute_address(exe, 0x80091F98, 0x80091F9C),
        sync_state=sync_state,
        ready_state=sync_state + 1,
        sync_callback=absolute_address(exe, 0x80091A44, 0x80091A48),
        ready_callback=absolute_address(exe, 0x80091A10, 0x80091A14),
        sync_result=absolute_address(exe, 0x80091A5C, 0x80091A60),
        ready_result=absolute_address(exe, 0x80091A28, 0x80091A2C),
        mmio_status_slot=absolute_address(exe, 0x80091314, 0x80091318),
        mmio_command_slot=absolute_address(exe, 0x800913AC, 0x800913B0),
        mmio_param_slot=absolute_address(exe, 0x80091418, 0x8009141C),
        mmio_irq_slot=absolute_address(exe, 0x80091330, 0x80091334),
        service_map=(
            (1, "ready=1 or error=5", "ready mask 4"),
            (2, "sync=2 or error=5", "sync mask 2"),
            (3, "sync=2/3 or error=5", "second-response mask 1 or sync mask 2"),
            (4, "ready=end=4", "ready mask 4"),
            (5, "ready=sync=5", "both mask 6"),
        ),
    )


def report_abi(abi: Abi) -> None:
    print("stock libcd command/completion ABI (verified retail instructions):")
    print(
        f"  CdControl/CdCommand 0x{abi.command:08X}(cmd, param, result, async) -> "
        f"sync 0x{abi.sync:08X}, service 0x{abi.service:08X}"
    )
    print(
        f"  command metadata: param-count table 0x{abi.param_count_table:08X}, "
        f"last-pos 0x{abi.last_position:08X}, mode 0x{abi.last_mode:08X}, "
        f"current cmd 0x{abi.current_command:08X}"
    )
    print(
        f"  completion state: sync 0x{abi.sync_state:08X}/result 0x{abi.sync_result:08X}/"
        f"callback 0x{abi.sync_callback:08X}; ready 0x{abi.ready_state:08X}/"
        f"result 0x{abi.ready_result:08X}/callback 0x{abi.ready_callback:08X}"
    )
    print(
        "  controller pointer slots: "
        f"status 0x{abi.mmio_status_slot:08X}, command/response 0x{abi.mmio_command_slot:08X}, "
        f"param/ack 0x{abi.mmio_param_slot:08X}, irq 0x{abi.mmio_irq_slot:08X}"
    )
    for reason, state, callback in abi.service_map:
        print(f"  INT{reason}: {state}; {callback}")


@dataclass(frozen=True)
class TraceVerdict:
    mode: int
    start_lba: int
    contiguous_sectors: int
    phase_runs: tuple[tuple[int, int], ...]
    total_sectors: int
    rate: int
    observation_seconds: int
    physical_upper_bound: int
    ack_seen: bool
    dma_seen: bool
    int1_statuses: tuple[int, ...]
    runaway: bool


COMMAND_RE = re.compile(
    r"\[cdc\]\s+cmd 0x([0-9A-Fa-f]{2}) params=\d+ \[([0-9A-Fa-f]{2}) "
    r"([0-9A-Fa-f]{2}) ([0-9A-Fa-f]{2})\]"
)
SECTOR_RE = re.compile(r"\[cdc\]\s+sector LBA (\d+) ")
WATCHDOG_RE = re.compile(
    r"\[watchdog\] armed: (\d+)s frame-progress timeout \((\d+)s grace"
)
CDCR_INT1_RE = re.compile(r"\[cdcr\]\s+r\[1803\]=E1\s")
CDCR_RESPONSE_RE = re.compile(r"\[cdcr\]\s+r\[1801\]=([0-9A-Fa-f]{2})\s")


def bcd(value: int) -> int:
    return (value >> 4) * 10 + (value & 0x0F)


def longest_contiguous(values: list[int]) -> tuple[int, int]:
    if not values:
        raise Refused("trace contains no delivered CDC sectors")
    best_start = current_start = values[0]
    best = current = 1
    for previous, value in itertools.pairwise(values):
        if value == previous + 1:
            current += 1
        else:
            current_start = value
            current = 1
        if current > best:
            best_start, best = current_start, current
    return best_start, best


def classify_trace(text: str) -> TraceVerdict:
    mode = None
    setloc = None
    active_phase: tuple[int, int, list[int]] | None = None
    phases: list[tuple[int, int, list[int]]] = []
    events = sorted(
        itertools.chain(COMMAND_RE.finditer(text), SECTOR_RE.finditer(text)),
        key=lambda match: match.start(),
    )
    for match in events:
        if match.re is SECTOR_RE:
            if active_phase is not None:
                active_phase[2].append(int(match.group(1)))
            continue

        command = int(match.group(1), 16)
        params = tuple(int(match.group(index), 16) for index in range(2, 5))
        if command == 0x02:
            setloc = params
            active_phase = None
        elif command == 0x0E:
            mode = params[0]
        elif command in (0x06, 0x1B) and mode is not None and setloc is not None:
            expected_lba = (
                (bcd(setloc[0]) * 60 + bcd(setloc[1])) * 75 + bcd(setloc[2]) - 150
            )
            active_phase = (expected_lba, mode, [])
            phases.append(active_phase)
    populated = [phase for phase in phases if phase[2]]
    if not populated:
        raise Refused(
            "trace does not contain a serviced Setloc, Setmode and ReadN/ReadS phase"
        )

    candidates = []
    phase_runs = []
    for expected_lba, phase_mode, sectors in populated:
        start_lba, contiguous = longest_contiguous(sectors)
        if start_lba != expected_lba:
            raise Refused(
                f"serviced phase starts at LBA {start_lba}, but its Setloc derives "
                f"LBA {expected_lba}"
            )
        candidates.append((contiguous, phase_mode, start_lba))
        phase_runs.append((start_lba, contiguous))
    contiguous, mode, start_lba = max(candidates)

    watchdog = WATCHDOG_RE.search(text)
    if not watchdog:
        raise Refused("trace has no watchdog timeout/grace denominator")
    timeout, grace = (int(value) for value in watchdog.groups())
    observation = timeout + grace
    rate = 150 if mode & 0x80 else 75
    int1_statuses = []
    awaiting_int1_response = False
    for line in text.splitlines():
        if CDCR_INT1_RE.search(line):
            awaiting_int1_response = True
            continue
        response = CDCR_RESPONSE_RE.search(line)
        if awaiting_int1_response and response:
            int1_statuses.append(int(response.group(1), 16))
            awaiting_int1_response = False
    # One opening sector may be ready at t=0; every later sector costs one drive interval.
    upper_bound = 1 + math.ceil(observation * rate)
    return TraceVerdict(
        mode=mode,
        start_lba=start_lba,
        contiguous_sectors=contiguous,
        phase_runs=tuple(phase_runs),
        total_sectors=sum(len(sectors) for _, _, sectors in populated),
        rate=rate,
        observation_seconds=observation,
        physical_upper_bound=upper_bound,
        ack_seen="w[1803]=07 bank=1" in text,
        dma_seen="DMA3 " in text,
        int1_statuses=tuple(int1_statuses),
        runaway=contiguous > upper_bound,
    )


def report_trace(verdict: TraceVerdict) -> None:
    answer = "IMPOSSIBLE UNPACED BURST" if verdict.runaway else "drive-rate bounded"
    print(
        f"trace: mode 0x{verdict.mode:02X} ({verdict.rate} sectors/s), LBA "
        f"{verdict.start_lba}, {verdict.contiguous_sectors} contiguous sector(s) in a "
        f"{verdict.observation_seconds}s watchdog window"
    )
    print(
        f"  physical upper bound {verdict.physical_upper_bound}; answer={answer}; "
        f"INT ack seen={verdict.ack_seen}; DMA service seen={verdict.dma_seen}"
    )
    phases = ", ".join(f"LBA {start} x {count}" for start, count in verdict.phase_runs)
    print(
        f"  serviced phases {len(verdict.phase_runs)}, total sectors {verdict.total_sectors}: "
        f"{phases}"
    )
    statuses = ", ".join(
        f"0x{status:02X}" for status in sorted(set(verdict.int1_statuses))
    )
    print(
        f"  observed INT1 response status(es): {statuses or 'not traced (enable cdcr)'}"
    )


def synthetic_trace(count: int) -> str:
    lines = [
        "[watchdog] armed: 1s frame-progress timeout (0s grace until ready)",
        "[cdc] cmd 0x02 params=3 [00 02 16]",
        "[cdc] cmd 0x0E params=1 [00 00 00]",
        "[cdc] cmd 0x06 params=0 [00 00 00]",
        "[cdcr] r[1803]=E1 bank=1 irq pc=0 ra=0",
        "[cdcr] r[1801]=22 bank=1 resp pc=0 ra=0",
        "[cdcw] w[1803]=07 bank=1 a0=0 s1=0 pc=0 ra=0",
    ]
    for lba in range(16, 16 + count):
        lines.append(
            f"[cdc] sector LBA {lba} file=0 chan=0 submode=0x00 audio=0 -> data FIFO"
        )
        lines.append(f"[cdc] DMA3 8 words -> 0x800BD9C0 (head now LBA {lba})")
    return "\n".join(lines)


def synthetic_multiphase_trace() -> str:
    lines = [
        "[watchdog] armed: 1s frame-progress timeout (0s grace until ready)",
        "[cdc] cmd 0x02 params=3 [00 02 16]",
        "[cdc] cmd 0x0E params=1 [00 00 00]",
        "[cdc] cmd 0x06 params=0 [00 00 00]",
        "[cdc] sector LBA 16 file=0 chan=0 submode=0x00 audio=0 -> data FIFO",
        "[cdc] cmd 0x02 params=3 [00 03 25]",
        "[cdc] cmd 0x06 params=0 [00 03 25]",
        "[cdc] sector LBA 100 file=0 chan=0 submode=0x00 audio=0 -> data FIFO",
        "[cdc] sector LBA 101 file=0 chan=0 submode=0x00 audio=0 -> data FIFO",
        "[cdcw] w[1803]=07 bank=1 a0=0 s1=0 pc=0 ra=0",
        "[cdc] DMA3 8 words -> 0x800BD9C0 (head now LBA 101)",
    ]
    return "\n".join(lines)


def selftest(exe_path: Path) -> int:
    results: list[tuple[str, bool, str]] = []
    try:
        verify_crt0.require_target(exe_path)
        abi = analyze(verify_crt0.load_exe(exe_path))
        results.append(
            (
                "positive: retail command, sync and service ABI derives",
                abi.command == CD_COMMAND and abi.service == CD_SERVICE,
                f"command=0x{abi.command:08X} service=0x{abi.service:08X}",
            )
        )
    except (verify_crt0.Refused, Refused) as exc:
        print(f"verify_cd_command --selftest: REFUSED — {exc}")
        return 2

    raw = bytearray(exe_path.read_bytes())
    psexe = verify_crt0.load_exe(exe_path)
    offset = 0x800 + 0x80091EA8 - psexe.load
    struct.pack_into("<I", raw, offset, 0)
    scratch = ROOT / "scratch" / "raw"
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(
        prefix="verify-cd-command-", dir=scratch
    ) as directory:
        mutated = Path(directory) / "mutated.exe"
        mutated.write_bytes(raw)
        try:
            analyze(verify_crt0.load_exe(mutated))
            results.append(
                (
                    "mutation negative: pre-sync call is authoritative",
                    False,
                    "accepted nop",
                )
            )
        except Refused as exc:
            results.append(
                (
                    "mutation negative: pre-sync call is authoritative",
                    "0x80091EA8" in str(exc),
                    str(exc),
                )
            )

    impossible = classify_trace(synthetic_trace(80))
    results.append(
        (
            "trace positive: faster-than-1x delivery is detected",
            impossible.runaway and impossible.ack_seen and impossible.dma_seen,
            f"{impossible.contiguous_sectors} > {impossible.physical_upper_bound}",
        )
    )
    bounded = classify_trace(synthetic_trace(2))
    results.append(
        (
            "opposite answer: a bounded serviced trace is accepted",
            not bounded.runaway and bounded.ack_seen and bounded.dma_seen,
            f"{bounded.contiguous_sectors} <= {bounded.physical_upper_bound}",
        )
    )
    results.append(
        (
            "trace status: first INT1 response is observable",
            bounded.int1_statuses == (0x22,),
            ", ".join(f"0x{status:02X}" for status in bounded.int1_statuses),
        )
    )
    multiphase = classify_trace(synthetic_multiphase_trace())
    results.append(
        (
            "multi-phase trace: later Setloc owns its own sector sequence",
            multiphase.phase_runs == ((16, 1), (100, 2))
            and multiphase.total_sectors == 3,
            f"longest phase LBA {multiphase.start_lba}, {multiphase.contiguous_sectors} sectors",
        )
    )

    passed = sum(ok for _, ok, _ in results)
    for name, ok, detail in results:
        print(f"  [{'PASS' if ok else 'FAIL'}] {name}\n         {detail}")
    print(f"verify_cd_command --selftest: {passed}/{len(results)} passed")
    return 0 if passed == len(results) else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, default=DEFAULT_EXE)
    parser.add_argument(
        "--trace", type=Path, help="classify a PSXPORT_DEBUG=cdc,cdcw,irq live log"
    )
    parser.add_argument("--expect", choices=("bounded", "runaway"))
    parser.add_argument(
        "--expect-int1-status",
        type=lambda value: int(value, 0),
        help="require a status byte observed after an INT1 cdcr trace",
    )
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    try:
        if args.selftest:
            return selftest(args.exe)
        verify_crt0.require_target(args.exe)
        report_abi(analyze(verify_crt0.load_exe(args.exe)))
        if args.trace:
            verdict = classify_trace(
                args.trace.read_text(encoding="utf-8", errors="replace")
            )
            report_trace(verdict)
            if args.expect and (verdict.runaway != (args.expect == "runaway")):
                actual = "runaway" if verdict.runaway else "bounded"
                print(f"verify_cd_command: expected {args.expect}, got {actual}")
                return 1
            if args.expect_int1_status is not None:
                if not verdict.int1_statuses:
                    raise Refused(
                        "--expect-int1-status requires PSXPORT_DEBUG=cdcr response reads"
                    )
                if args.expect_int1_status not in verdict.int1_statuses:
                    observed = ", ".join(
                        f"0x{status:02X}"
                        for status in sorted(set(verdict.int1_statuses))
                    )
                    print(
                        f"verify_cd_command: expected INT1 status "
                        f"0x{args.expect_int1_status:02X}, observed {observed}"
                    )
                    return 1
        elif args.expect or args.expect_int1_status is not None:
            raise Refused("--expect and --expect-int1-status require --trace")
        return 0
    except (OSError, verify_crt0.Refused, Refused) as exc:
        print(f"verify_cd_command: REFUSING — {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
