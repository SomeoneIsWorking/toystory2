#!/usr/bin/env python3
"""Verify Toy Story 2's captured-queue frame fence and first title frame.

The historical guest-owned loop submitted draw lists from its VBlank callback.
The current title-owned resident loop closes the shared capture once after its
two measured display fields; calling the lower-level presenter still leaves
every flush in one capture forever.
This gate distinguishes that missing fence from a legitimately large queue or
a runaway producer, then checks a real post-fix trace and title capture.
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_FRAME_CLOCK = ROOT / "game" / "loop" / "toystory2_frame_driver.cpp"
DEFAULT_OUTER_LOOP_HEADER = ROOT / "game" / "loop" / "outer_loop.h"
DEFAULT_SYNC_OVERRIDES = ROOT / "game" / "boot" / "native_sync_overrides.cpp"
DEFAULT_GENERATED_RESIDENT = ROOT / "generated" / "shard_3.c"
DEFAULT_QUEUE_HEADER = (
    ROOT / "external" / "psxport" / "runtime" / "recomp" / "render_queue.h"
)
DEFAULT_PRE_LOG = ROOT / "scratch" / "logs" / "frame-fence-pre.log"
DEFAULT_POST_LOG = ROOT / "scratch" / "logs" / "frame-fence-final.log"
# Re-measured 2026-08-24 after RE-14's seed advanced the route: the title is on
# screen by present 1500 and has transitioned away again before 2400, so the
# title/negative control sets moved from (2100, 2400)/(900, 1500).
DEFAULT_TITLE_SHOTS = (
    ROOT / "scratch" / "screenshots" / "present_1500.ppm",
    ROOT / "scratch" / "screenshots" / "present_2100.ppm",
)
DEFAULT_NEGATIVE_SHOTS = (
    ROOT / "scratch" / "screenshots" / "present_900.ppm",
    ROOT / "scratch" / "screenshots" / "present_2400.ppm",
)
# Historical RE-14 frame-fence witness: this trace predates RE-16's emitter correction and therefore
# ends at the now-retired model-pointer fault. Requiring its exact terminal address and consumer keeps
# this frame-fence artifact classifiable without relabelling it as the current frontier. Current
# reset/continuation evidence belongs to verify_model_table_reset.py.
NEXT_BOUNDARY_EVIDENCE = (
    "[mem:error] FATAL: UNMAPPED RAM read16 @ 0xEDA4F893 "
    "(phys 0x0DA4F893) — fail-fast."
)
NEXT_BOUNDARY_FUNCTION = "[mem:error]   guest: last-fn-entered=0x800426E0"
RETIRED_MISS = "no recompiled fn for 0x800D12C4"

FLUSH_RE = re.compile(r"^\[rqflush\] n=(\d+) reemit=([01]) ", re.MULTILINE)
COMMIT_RE = re.compile(r"^\[ts2-field\] committed captured guest field$", re.MULTILINE)
OVERFLOW_RE = re.compile(
    r"Fps60::rq_capture OVERFLOW: (\d+) captured \+ (\d+) this flush > "
    r"FPS60_RQ_MAX (\d+)"
)


class Refused(Exception):
    """The supplied source, trace, or image does not prove the claim."""


@dataclass(frozen=True)
class TraceEvidence:
    flushes: int
    new_flushes: int
    reemits: int
    commits: int
    max_flush: int
    max_frame_capture: int
    overflow: tuple[int, int, int] | None


@dataclass(frozen=True)
class TitleEvidence:
    width: int
    height: int
    pixels: int
    blue: int
    yellow: int
    red: int
    green: int


def queue_capacity(header: str) -> int:
    match = re.search(r"^#define RQ_MAX (\d+)$", header, re.MULTILINE)
    if not match:
        raise Refused("framework render_queue.h has no decimal RQ_MAX authority")
    return int(match.group(1))


def check_shipping_source(source: str) -> None:
    if "core_.game->presentation.commit(&core_, guestFields);" not in source:
        raise Refused(
            "Toy Story 2's native frame boundary does not use the neutral title-owned fence"
        )
    if "rec_dispatch(&core_, residentUpdateAddress(alternate));" not in source:
        raise Refused("Toy Story 2's frame boundary does not route the measured resident owners")
    if 'constexpr uint32_t kLibetcVBlankCount = 0x8009FD54u;' not in source:
        raise Refused("Toy Story 2's frame boundary does not name its linked-libetc field counter")
    if "residentPreparation_.step(core_, level" not in source:
        raise Refused(
            "resident preparation drops the selected-level a0 live into retail 0x8007BEC4"
        )
    if "rec_dispatch(&core_, kGuestVBlankHandler)" in source:
        raise Refused("Toy Story 2 still dispatches guest VBlank from its host frame owner")
    if "rec_host_turn_register" in source:
        raise Refused("Toy Story 2 still registers callback-driven host turns")
    if "core->game->fps60.frame_commit" in source:
        raise Refused("Toy Story 2 directly couples its field fence to Fps60")
    if re.search(r"^\s*gpu_present\([^;]+\);", source, re.MULTILINE):
        raise Refused(
            "Toy Story 2 still bypasses the frame fence with the lower-level presenter"
        )


def check_native_loop_ownership(
    outer_loop: str, sync_overrides: str, generated: str | None
) -> None:
    if "alternateMode ? 0x8007B850u : 0x8007B254u" not in outer_loop:
        raise Refused("finite outer loop does not retain both measured resident owners")
    for address in ("0x8003A218u", "0x80039D9Cu", "0x8003FA68u"):
        if f"registry->shard_set_override({address}" not in sync_overrides:
            raise Refused(f"native synchronization override {address} is not registered")
    if "core.mem_w16(0x800A1E64u, 0xFFFF);" not in sync_overrides:
        raise Refused("graphics-buffer swap does not clear retail gp+4492")
    if "0x80088628u" in sync_overrides:
        raise Refused("title synchronization override still names or dispatches guest VSync")
    if generated is not None:
        graphics_super = re.search(
            r"void gen_func_8003A218\(Core\* c\) \{(?P<body>.*?)\n\}\n\nvoid gen_func_",
            generated,
            re.DOTALL,
        )
        if graphics_super is None or graphics_super.group("body").count("func_80088628(c);") != 2:
            raise Refused("generated graphics-init super no longer contains its two retail VSync calls")


def analyze_trace(log: str) -> TraceEvidence:
    flushes = 0
    new_flushes = 0
    reemits = 0
    max_flush = 0
    current_capture = 0
    max_frame_capture = 0
    commits = 0

    for line in log.splitlines():
        flush = FLUSH_RE.match(line)
        if flush:
            count = int(flush.group(1))
            reemit = int(flush.group(2))
            flushes += 1
            new_flushes += 1 - reemit
            reemits += reemit
            max_flush = max(max_flush, count)
            current_capture += count
        commit = COMMIT_RE.match(line)
        if commit:
            commits += 1
            max_frame_capture = max(max_frame_capture, current_capture)
            current_capture = 0

    overflow_match = OVERFLOW_RE.search(log)
    overflow = (
        tuple(int(value) for value in overflow_match.groups())
        if overflow_match
        else None
    )
    return TraceEvidence(
        flushes=flushes,
        new_flushes=new_flushes,
        reemits=reemits,
        commits=commits,
        max_flush=max_flush,
        max_frame_capture=max_frame_capture,
        overflow=overflow,
    )


def classify_pre_fix(evidence: TraceEvidence, capacity: int) -> None:
    if evidence.overflow is None:
        raise Refused("pre-fix trace has no counted capture overflow")
    captured, incoming, reported_capacity = evidence.overflow
    if reported_capacity != capacity or captured + incoming <= capacity:
        raise Refused(
            "pre-fix overflow does not cross the framework's recorded capacity"
        )
    if evidence.commits:
        raise Refused("pre-fix control unexpectedly contains frame commits")
    if evidence.flushes < 2 or evidence.max_flush >= capacity:
        raise Refused(
            "pre-fix trace does not distinguish accumulation from one giant flush"
        )
    if evidence.reemits <= evidence.new_flushes:
        raise Refused(
            "pre-fix trace does not show repeated presentation dominating production"
        )


def classify_post_fix(log: str, evidence: TraceEvidence, capacity: int) -> None:
    if evidence.overflow is not None:
        raise Refused("post-fix trace still overflows the captured queue")
    if evidence.commits < 2:
        raise Refused("post-fix trace lacks repeated field-boundary commits")
    if evidence.max_frame_capture <= 0 or evidence.max_frame_capture >= capacity:
        raise Refused(
            "post-fix per-frame capture is empty or still reaches queue capacity"
        )
    if evidence.max_flush >= capacity:
        raise Refused(
            "post-fix trace contains a genuinely over-capacity producer flush"
        )
    if NEXT_BOUNDARY_EVIDENCE not in log:
        raise Refused(
            f"post-fix trace does not reach its historical terminal evidence "
            f"'{NEXT_BOUNDARY_EVIDENCE}'"
        )
    if NEXT_BOUNDARY_FUNCTION not in log:
        raise Refused(
            "post-fix trace reaches the historical address but not through the classified "
            "model-pointer consumer at 0x800426E0"
        )
    if RETIRED_MISS in log:
        raise Refused(
            "post-fix trace regressed to the retired function-discovery miss at "
            f"0x{0x800D12C4:08X} — the seeded overlay entry did not execute"
        )


def ppm_pixels(blob: bytes) -> tuple[int, int, bytes]:
    match = re.match(rb"P6\s+(\d+)\s+(\d+)\s+255\s", blob)
    if not match:
        raise Refused("title evidence is not an 8-bit binary PPM")
    width, height = (int(value) for value in match.groups())
    pixels = blob[match.end() :]
    if len(pixels) != width * height * 3:
        raise Refused("title PPM payload size disagrees with its dimensions")
    return width, height, pixels


def analyze_title(blob: bytes) -> TitleEvidence:
    width, height, payload = ppm_pixels(blob)
    blue = yellow = red = green = 0
    for offset in range(0, len(payload), 3):
        r, g, b = payload[offset : offset + 3]
        blue += b > 80 and b > r * 1.25 and b > g * 1.10
        yellow += r > 150 and g > 110 and b < 100
        red += r > 130 and r > g * 1.50 and r > b * 1.50
        green += g > 80 and g > r * 1.20 and g > b * 1.10
    return TitleEvidence(width, height, width * height, blue, yellow, red, green)


def classify_title(evidence: TitleEvidence) -> None:
    # Measured color-family discriminator for the retail title: its blue field,
    # yellow logo, red logo panel and green Buzz suit must all be present. Real
    # early-black and transition captures from the same run are the controls.
    required = {
        "blue": (evidence.blue, 0.50),
        "yellow": (evidence.yellow, 0.02),
        "red": (evidence.red, 0.01),
        "green": (evidence.green, 0.005),
    }
    failed = [
        name
        for name, (count, fraction) in required.items()
        if count < evidence.pixels * fraction
    ]
    if failed:
        raise Refused("capture lacks title color families: " + ", ".join(failed))


def check(
    source_path: Path,
    outer_loop_path: Path,
    sync_overrides_path: Path,
    generated_path: Path,
    header_path: Path,
    pre_log_path: Path,
    post_log_path: Path,
    title_shot_paths: tuple[Path, ...],
    negative_shot_paths: tuple[Path, ...],
) -> None:
    check_shipping_source(source_path.read_text(encoding="utf-8"))
    check_native_loop_ownership(
        outer_loop_path.read_text(encoding="utf-8"),
        sync_overrides_path.read_text(encoding="utf-8"),
        generated_path.read_text(encoding="utf-8") if generated_path.exists() else None,
    )
    capacity = queue_capacity(header_path.read_text(encoding="utf-8"))
    pre = analyze_trace(pre_log_path.read_text(encoding="utf-8", errors="replace"))
    post_text = post_log_path.read_text(encoding="utf-8", errors="replace")
    post = analyze_trace(post_text)
    classify_pre_fix(pre, capacity)
    classify_post_fix(post_text, post, capacity)
    titles = []
    for path in title_shot_paths:
        title = analyze_title(path.read_bytes())
        classify_title(title)
        titles.append((path, title))
    for path in negative_shot_paths:
        try:
            classify_title(analyze_title(path.read_bytes()))
        except Refused:
            continue
        raise Refused(f"negative capture was misclassified as the title: {path}")
    print(
        "[frame-fence] pre-fix: "
        f"{pre.flushes} flushes ({pre.reemits} re-emits), no commits, "
        f"max flush {pre.max_flush}, accumulated to {pre.overflow[0]}+{pre.overflow[1]} "
        f"> {capacity}"
    )
    print(
        "[frame-fence] post-fix: "
        f"{post.commits} commits, max flush {post.max_flush}, max captured field "
        f"{post.max_frame_capture}; next boundary '{NEXT_BOUNDARY_EVIDENCE}'"
    )
    for path, title in titles:
        print(
            f"[frame-fence] title {path.name}: "
            f"{title.width}x{title.height}, blue={title.blue}, "
            f"yellow={title.yellow}, red={title.red}, green={title.green}"
        )
    print(
        "[frame-fence] title negatives rejected: "
        + ", ".join(path.name for path in negative_shot_paths)
    )


def selftest(
    source_path: Path,
    outer_loop_path: Path,
    sync_overrides_path: Path,
    generated_path: Path,
    header_path: Path,
) -> int:
    checks: list[tuple[str, bool]] = []

    def expect(name: str, operation, accepted: bool) -> None:
        try:
            operation()
            result = True
        except (OSError, Refused):
            result = False
        checks.append((name, result == accepted))

    generated_fixture = (
        "void gen_func_8003A218(Core* c) {\n"
        "func_80088628(c);\nfunc_80088628(c);\n}\n\n"
        "void gen_func_8003A548(Core* c) {}\n"
    )
    generated_source = (
        generated_path.read_text(encoding="utf-8")
        if generated_path.exists()
        else generated_fixture
    )

    expect(
        "positive shipping frame-fence ownership",
        lambda: check_shipping_source(source_path.read_text(encoding="utf-8")),
        True,
    )
    expect(
        "negative resident preparation drops selected-level a0",
        lambda: check_shipping_source(
            source_path.read_text(encoding="utf-8").replace(
                "residentPreparation_.step(core_, level",
                "residentPreparation_.step(core_, 0",
            )
        ),
        False,
    )
    expect(
        "positive native loop owners preserve generated VSync super",
        lambda: check_native_loop_ownership(
            outer_loop_path.read_text(encoding="utf-8"),
            sync_overrides_path.read_text(encoding="utf-8"),
            generated_source,
        ),
        True,
    )
    expect(
        "negative missing alternate resident owner",
        lambda: check_native_loop_ownership(
            "return 0x8007B254u;",
            sync_overrides_path.read_text(encoding="utf-8"),
            generated_source,
        ),
        False,
    )
    expect(
        "negative wrong graphics-buffer state address",
        lambda: check_native_loop_ownership(
            outer_loop_path.read_text(encoding="utf-8"),
            sync_overrides_path.read_text(encoding="utf-8").replace(
                "core.mem_w16(0x800A1E64u, 0xFFFF);",
                "core.mem_w16(0x800A118Cu, 0xFFFF);",
            ),
            generated_source,
        ),
        False,
    )
    expect(
        "negative edited generated VSync super",
        lambda: check_native_loop_ownership(
            outer_loop_path.read_text(encoding="utf-8"),
            sync_overrides_path.read_text(encoding="utf-8"),
            generated_source.replace("func_80088628(c);", "", 1),
        ),
        False,
    )
    expect(
        "negative direct temporal-decorator coupling",
        lambda: check_shipping_source("core_.game->fps60.frame_commit(&core_, 2);\n"),
        False,
    )
    expect(
        "negative lower-level presentation bypass",
        lambda: check_shipping_source(
            "rec_dispatch(&core_, residentUpdateAddress(alternate));\n"
            'constexpr uint32_t kLibetcVBlankCount = 0x8009FD54u;\n'
            "core_.game->presentation.commit(&core_, guestFields);\n"
            "gpu_present(&core_);\n"
        ),
        False,
    )
    expect(
        "negative guest-vblank dispatch",
        lambda: check_shipping_source(
            "rec_dispatch(&core_, residentUpdateAddress(alternate));\n"
            'constexpr uint32_t kLibetcVBlankCount = 0x8009FD54u;\n'
            "core_.game->presentation.commit(&core_, guestFields);\n"
            "rec_dispatch(&core_, kGuestVBlankHandler);\n"
        ),
        False,
    )
    expect(
        "negative callback-driven host turn",
        lambda: check_shipping_source(
            "rec_dispatch(&core_, residentUpdateAddress(alternate));\n"
            'constexpr uint32_t kLibetcVBlankCount = 0x8009FD54u;\n'
            "core_.game->presentation.commit(&core_, guestFields);\n"
            "rec_host_turn_register(&core_, turn, 59940);\n"
        ),
        False,
    )
    capacity = queue_capacity(header_path.read_text(encoding="utf-8"))
    pre = "".join("[rqflush] n=1200 reemit=1 seq=1200 y=[0..1]\n" for _ in range(55))
    pre += f"[fps60:error] Fps60::rq_capture OVERFLOW: 64800 captured + 1200 this flush > FPS60_RQ_MAX {capacity}.\n"
    expect(
        "positive accumulation classification",
        lambda: classify_pre_fix(analyze_trace(pre), capacity),
        True,
    )
    expect(
        "negative one giant producer is not missing-fence evidence",
        lambda: classify_pre_fix(
            analyze_trace(
                f"[rqflush] n={capacity} reemit=0 seq=1 y=[0..1]\n"
                f"[fps60:error] Fps60::rq_capture OVERFLOW: 1 captured + {capacity} this flush > FPS60_RQ_MAX {capacity}.\n"
            ),
            capacity,
        ),
        False,
    )
    post = (
        "[rqflush] n=1200 reemit=0 seq=1200 y=[0..1]\n"
        "[ts2-field] committed captured guest field\n"
        "[rqflush] n=1200 reemit=1 seq=1200 y=[0..1]\n"
        "[ts2-field] committed captured guest field\n"
        f"{NEXT_BOUNDARY_FUNCTION} (NOT the faulting pc — see comment)\n"
        f"{NEXT_BOUNDARY_EVIDENCE}\n"
    )
    expect(
        "positive committed route reaches next boundary",
        lambda: classify_post_fix(post, analyze_trace(post), capacity),
        True,
    )
    expect(
        "negative right fault through wrong consumer",
        lambda: classify_post_fix(
            post.replace("last-fn-entered=0x800426E0", "last-fn-entered=0x800426DC"),
            analyze_trace(post),
            capacity,
        ),
        False,
    )
    expect(
        "negative right consumer with wrong fault address",
        lambda: classify_post_fix(
            post.replace("0xEDA4F893", "0xEDA4F891").replace(
                "0x0DA4F893", "0x0DA4F891"
            ),
            analyze_trace(post),
            capacity,
        ),
        False,
    )
    retired = (
        "[rqflush] n=1200 reemit=0 seq=1200 y=[0..1]\n"
        "[ts2-field] committed captured guest field\n"
        "[rqflush] n=1200 reemit=1 seq=1200 y=[0..1]\n"
        "[ts2-field] committed captured guest field\n"
        "[hle:warn] [recomp-MISS 0] no recompiled fn for 0x800D12C4\n"
    )
    expect(
        "negative retired miss means the seeded entry did not execute",
        lambda: classify_post_fix(retired, analyze_trace(retired), capacity),
        False,
    )
    expect(
        "negative overflow survives frame fence",
        lambda: classify_post_fix(
            post
            + f"[fps60:error] Fps60::rq_capture OVERFLOW: 1 captured + {capacity} this flush > FPS60_RQ_MAX {capacity}.\n",
            analyze_trace(
                post
                + f"[fps60:error] Fps60::rq_capture OVERFLOW: 1 captured + {capacity} this flush > FPS60_RQ_MAX {capacity}.\n"
            ),
            capacity,
        ),
        False,
    )
    title_pixels = bytes([10, 20, 180]) * 60 + bytes([240, 190, 20]) * 20
    title_pixels += bytes([220, 20, 20]) * 10 + bytes([20, 180, 20]) * 10
    expect(
        "positive title color families",
        lambda: classify_title(analyze_title(b"P6\n10 10\n255\n" + title_pixels)),
        True,
    )
    expect(
        "negative monochrome card is not the title",
        lambda: classify_title(
            analyze_title(b"P6\n10 10\n255\n" + bytes([240, 240, 240]) * 100)
        ),
        False,
    )

    for name, passed in checks:
        print(f"[selftest] {'PASS' if passed else 'FAIL'} {name}")
    print(f"[selftest] {sum(passed for _, passed in checks)}/{len(checks)} passed")
    return 0 if all(passed for _, passed in checks) else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_mutually_exclusive_group(required=True)
    modes.add_argument("--check", action="store_true")
    modes.add_argument("--selftest", action="store_true")
    parser.add_argument("--source", type=Path, default=DEFAULT_FRAME_CLOCK)
    parser.add_argument("--outer-loop", type=Path, default=DEFAULT_OUTER_LOOP_HEADER)
    parser.add_argument("--sync-overrides", type=Path, default=DEFAULT_SYNC_OVERRIDES)
    parser.add_argument("--generated-resident", type=Path, default=DEFAULT_GENERATED_RESIDENT)
    parser.add_argument("--queue-header", type=Path, default=DEFAULT_QUEUE_HEADER)
    parser.add_argument("--pre-log", type=Path, default=DEFAULT_PRE_LOG)
    parser.add_argument("--post-log", type=Path, default=DEFAULT_POST_LOG)
    parser.add_argument("--title-shot", action="append", type=Path)
    parser.add_argument("--reject-shot", action="append", type=Path)
    args = parser.parse_args()
    try:
        if args.selftest:
            return selftest(
                args.source,
                args.outer_loop,
                args.sync_overrides,
                args.generated_resident,
                args.queue_header,
            )
        check(
            args.source,
            args.outer_loop,
            args.sync_overrides,
            args.generated_resident,
            args.queue_header,
            args.pre_log,
            args.post_log,
            tuple(args.title_shot or DEFAULT_TITLE_SHOTS),
            tuple(args.reject_shot or DEFAULT_NEGATIVE_SHOTS),
        )
        return 0
    except (OSError, Refused) as error:
        print(f"[frame-fence] REFUSED: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
