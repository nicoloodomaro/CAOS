#!/usr/bin/env python3
import argparse
import re
import signal
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Dict, List, Tuple


@dataclass(frozen=True)
class TestResult:
    passed: bool
    detail: str


@dataclass(frozen=True)
class TestSpec:
    test_id: int
    title: str
    checker: Callable[[str], TestResult]


def _contains(log: str, token: str) -> bool:
    return token in log


def _count_deadline_miss(log: str) -> int:
    return log.count("deadline miss")


def _task_miss(log: str, task_name: str) -> bool:
    return _contains(log, f"[{task_name}] terminated -> deadline miss")


def _task_completed(log: str, task_name: str) -> bool:
    return _contains(log, f"[{task_name}] completed")


def _missing_tokens(log: str, tokens: List[str]) -> List[str]:
    return [token for token in tokens if token not in log]


def _tick_for_event(log: str, event_token: str) -> int:
    current_tick = -1
    tick_re = re.compile(r"^tick\s+(\d+):")

    for raw_line in log.splitlines():
        line = raw_line.strip()
        match = tick_re.match(line)
        if match:
            current_tick = int(match.group(1))
            continue

        if event_token in line:
            return current_tick

    return -1


def _check_common_boot(log: str) -> TestResult:
    required = ["tick 0:", "Start major frame 0"]
    missing = _missing_tokens(log, required)
    if missing:
        return TestResult(False, f"Missing boot markers: {', '.join(missing)}")
    return TestResult(True, "")


def check_test_1(log: str) -> TestResult:
    boot = _check_common_boot(log)
    if not boot.passed:
        return boot

    expected_misses = ["HRT_B", "HRT_C", "HRT_D", "HRT_E"]
    missing = [name for name in expected_misses if not _task_miss(log, name)]
    if missing:
        return TestResult(False, f"Expected overlap misses not found: {', '.join(missing)}")

    if not _task_completed(log, "HRT_A"):
        return TestResult(False, "HRT_A did not complete as expected")

    return TestResult(True, f"{_count_deadline_miss(log)} deadline misses observed (overlap stress OK)")


def check_test_2(log: str) -> TestResult:
    boot = _check_common_boot(log)
    if not boot.passed:
        return boot

    for task_name in ["HRT_A", "HRT_B", "HRT_C", "HRT_D"]:
        if not _task_completed(log, task_name):
            return TestResult(False, f"{task_name} did not complete in minimal-gap scenario")
        if _task_miss(log, task_name):
            return TestResult(False, f"{task_name} unexpectedly missed deadline")

    if not _task_miss(log, "HRT_E"):
        return TestResult(False, "HRT_E expected miss was not observed")

    return TestResult(True, "Edge-case gap behavior preserved (A-D complete, E misses)")


def check_test_3(log: str) -> TestResult:
    boot = _check_common_boot(log)
    if not boot.passed:
        return boot

    if _count_deadline_miss(log) != 0:
        return TestResult(False, "Unexpected deadline miss detected in preemption chain")

    required_switches = [
        "event: context switch SRT_X -> HRT_A",
        "event: context switch SRT_X -> HRT_B",
        "event: context switch SRT_X -> HRT_C",
        "event: context switch SRT_X -> HRT_D",
    ]
    missing = _missing_tokens(log, required_switches)
    if missing:
        return TestResult(False, f"Missing expected preemptions: {', '.join(missing)}")

    return TestResult(True, "Repeated SRT->HRT preemptions observed with no misses")


def check_test_4(log: str) -> TestResult:
    boot = _check_common_boot(log)
    if not boot.passed:
        return boot

    if _count_deadline_miss(log) != 0:
        return TestResult(False, "Unexpected deadline miss in deterministic timing profile")

    for task_name in ["HRT_A", "HRT_B", "HRT_C", "HRT_D", "SRT_X", "SRT_Y", "SRT_Z"]:
        if not _task_completed(log, task_name):
            return TestResult(False, f"{task_name} completion not observed")

    return TestResult(True, "Deterministic timing sequence completed without misses")


def check_test_5(log: str) -> TestResult:
    boot = _check_common_boot(log)
    if not boot.passed:
        return boot

    for task_name in ["HRT_A", "HRT_B"]:
        if not _task_miss(log, task_name):
            return TestResult(False, f"{task_name} expected deadline miss not observed")

    for task_name in ["HRT_C", "HRT_D"]:
        if not _task_completed(log, task_name):
            return TestResult(False, f"{task_name} expected completion not observed")

    return TestResult(True, "Intentional miss-and-recovery pattern matches expected behavior")


def check_test_6(log: str) -> TestResult:
    boot = _check_common_boot(log)
    if not boot.passed:
        return boot

    if not _task_miss(log, "HRT_D"):
        return TestResult(False, "HRT_D expected miss not observed")

    for task_name in ["HRT_A", "HRT_B", "HRT_C"]:
        if not _task_completed(log, task_name):
            return TestResult(False, f"{task_name} expected completion not observed")

    return TestResult(True, "Mixed-load profile stable (single expected miss on HRT_D)")


def check_test_7(log: str) -> TestResult:
    boot = _check_common_boot(log)
    if not boot.passed:
        return boot

    for task_name in ["HRT_MISS_A", "HRT_MISS_B", "HRT_PASS_SF", "HRT_MAJ_MISS", "SRT_MAJ_MISS"]:
        if not _task_miss(log, task_name):
            return TestResult(False, f"Expected miss for {task_name} not observed")

    if not _task_completed(log, "SRT_SHORT"):
        return TestResult(False, "SRT_SHORT completion missing after major-frame reset")

    return TestResult(True, "All injected failure modes detected (subframe + major-frame misses)")


def check_test_8(log: str) -> TestResult:
    boot = _check_common_boot(log)
    if not boot.passed:
        return boot

    if _count_deadline_miss(log) != 0:
        return TestResult(False, "Unexpected miss in schedulable even profile")

    for task_name in ["HRT_A", "HRT_B", "HRT_C", "HRT_D"]:
        if not _task_completed(log, task_name):
            return TestResult(False, f"{task_name} completion missing")

    return TestResult(True, "Schedulable even profile completed with zero misses")


def check_test_9(log: str) -> TestResult:
    boot = _check_common_boot(log)
    if not boot.passed:
        return boot

    for task_name in ["HRT_CH1", "HRT_CH2", "HRT_PASS_SF", "HRT_MAJ_MISS", "SRT_MAJ_MISS"]:
        if not _task_miss(log, task_name):
            return TestResult(False, f"Expected miss for {task_name} not observed")

    if not _task_completed(log, "SRT_SHORT"):
        return TestResult(False, "SRT_SHORT completion missing")

    return TestResult(True, "Failure-chain profile detected all targeted misses")


def check_test_10(log: str) -> TestResult:
    boot = _check_common_boot(log)
    if not boot.passed:
        return boot

    if _count_deadline_miss(log) != 0:
        return TestResult(False, "Unexpected miss in schedulable even profile")

    for task_name in ["HRT_A", "HRT_B", "HRT_C", "HRT_D"]:
        if not _task_completed(log, task_name):
            return TestResult(False, f"{task_name} completion missing")

    return TestResult(True, "Long major frame remains schedulable with no misses")


def check_test_11(log: str) -> TestResult:
    boot = _check_common_boot(log)
    if not boot.passed:
        return boot

    for task_name in ["HRT_TIGHT", "HRT_BLOCKED", "HRT_PASS_SF", "HRT_MAJ_MISS", "SRT_MAJ_MISS"]:
        if not _task_miss(log, task_name):
            return TestResult(False, f"Expected miss for {task_name} not observed")

    if not _task_completed(log, "SRT_AUX"):
        return TestResult(False, "SRT_AUX completion missing")

    return TestResult(True, "Injected tight/blocked/subframe/major misses all reproduced")


TEST_SPECS: Dict[int, TestSpec] = {
    1: TestSpec(1, "Overlapping HRT Stress", check_test_1),
    2: TestSpec(2, "Minimal Gap Edge Case", check_test_2),
    3: TestSpec(3, "SRT Preemption Chain", check_test_3),
    4: TestSpec(4, "Timing Consistency", check_test_4),
    5: TestSpec(5, "Deadline-Miss Recovery", check_test_5),
    6: TestSpec(6, "Mixed Load Baseline", check_test_6),
    7: TestSpec(7, "Failure Injection A", check_test_7),
    8: TestSpec(8, "Schedulable Even Profile A", check_test_8),
    9: TestSpec(9, "Failure Injection B", check_test_9),
    10: TestSpec(10, "Schedulable Even Profile B", check_test_10),
    11: TestSpec(11, "Failure Injection C", check_test_11),
}

_MAJOR_FRAME_LINE_RE = re.compile(r"Start major frame\s+(\d+)")
_FIRST_EXCLUDED_FRAME_ID = 2


def _run(cmd: List[str], cwd: Path, capture: bool) -> subprocess.CompletedProcess:
    return subprocess.run(
        cmd,
        cwd=str(cwd),
        check=False,
        text=True,
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.STDOUT if capture else None,
    )


def _build_profile(app_dir: Path, profile: int) -> Tuple[bool, str]:
    cmd = [
        "make",
        "cleanobjs",
        "all",
        f"PROFILE={profile}",
        "TL_DEBUG=1",
    ]
    result = _run(cmd, app_dir, capture=True)
    if result.returncode != 0:
        return False, result.stdout or ""
    return True, result.stdout or ""


def _run_qemu_capture(app_dir: Path, timeout_s: float, live: bool) -> str:
    elf_path = app_dir / "Output" / "timeline_demo.elf"
    qemu_cmd = [
        "qemu-system-arm",
        "-machine",
        "mps2-an385",
        "-cpu",
        "cortex-m3",
        "-kernel",
        str(elf_path),
        "-monitor",
        "none",
        "-nographic",
        "-serial",
        "stdio",
    ]

    log_lines: List[str] = []
    start_time = time.monotonic()
    process = subprocess.Popen(
        qemu_cmd,
        cwd=str(app_dir),
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        bufsize=1,
    )

    try:
        while True:
            if (time.monotonic() - start_time) >= timeout_s:
                break

            if process.stdout is None:
                break

            line = process.stdout.readline()
            if line == "":
                if process.poll() is not None:
                    break
                time.sleep(0.002)
                continue

            match = _MAJOR_FRAME_LINE_RE.search(line)
            if (match is not None) and (int(match.group(1)) >= _FIRST_EXCLUDED_FRAME_ID):
                break

            log_lines.append(line)
            if live:
                print(line, end="")
    finally:
        if process.poll() is None:
            process.send_signal(signal.SIGINT)
            try:
                process.wait(timeout=0.3)
            except subprocess.TimeoutExpired:
                process.terminate()
                try:
                    process.wait(timeout=0.3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=0.3)

    return "".join(log_lines)


def _format_result(spec: TestSpec, result: TestResult) -> str:
    status = "PASSED" if result.passed else "FAILED"
    return f"Test {spec.test_id} - {spec.title}: {status} ({result.detail})"


def run_test(app_dir: Path, spec: TestSpec, timeout_s: float, live: bool) -> TestResult:
    build_ok, _ = _build_profile(app_dir, spec.test_id)
    if not build_ok:
        return TestResult(False, "Build failed")

    log = _run_qemu_capture(app_dir, timeout_s, live=live)

    result = spec.checker(log)
    if not result.passed:
        miss_tick = _tick_for_event(log, "deadline miss")
        if miss_tick >= 0:
            return TestResult(False, f"{result.detail}; first miss near tick {miss_tick}")
    return result


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Timeline scheduler regression suite")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--all", action="store_true", help="Run all tests sequentially")
    mode.add_argument("--test", type=int, help="Run one specific test id")
    parser.add_argument("--timeout", type=float, default=3.0, help="Per-test QEMU timeout in seconds")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    app_dir = Path(__file__).resolve().parents[1]

    if args.all:
        selected = [TEST_SPECS[idx] for idx in sorted(TEST_SPECS.keys())]
        live = False
    else:
        if args.test not in TEST_SPECS:
            print(f"Unsupported test id {args.test}. Use 1..11.", file=sys.stderr)
            return 2
        selected = [TEST_SPECS[args.test]]
        live = True

    passed = 0
    total = len(selected)

    for index, spec in enumerate(selected, start=1):
        try:
            result = run_test(app_dir, spec, timeout_s=args.timeout, live=live)
        except KeyboardInterrupt:
            if args.all:
                print(f"\nInterrupted by user after {index - 1}/{total} tests")
            else:
                print("\nInterrupted by user")
            return 130
        print(_format_result(spec, result))
        if result.passed:
            passed += 1

    if args.all:
        print(f"Summary: {passed}/{total} tests passed")

    return 0 if passed == total else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\nInterrupted by user")
        sys.exit(130)
