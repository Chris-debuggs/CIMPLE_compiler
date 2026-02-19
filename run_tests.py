#!/usr/bin/env python3
"""Oracle harness for CIMPLE.

For each .cimp program:
1) Run evaluator (`cimple run`) -> expected output
2) Build native executable (`cimple build`) and run it -> actual output
3) Compare stdout byte-for-byte
"""

from __future__ import annotations

import argparse
import difflib
import os
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Optional


IS_WINDOWS = os.name == "nt"


@dataclass
class CommandResult:
    argv: list[str]
    returncode: int
    stdout: bytes
    stderr: bytes
    timed_out: bool = False


def decode(data: bytes) -> str:
    return data.decode("utf-8", errors="replace")


def run_command(argv: list[str], cwd: Path, timeout: float) -> CommandResult:
    try:
        proc = subprocess.run(
            argv,
            cwd=str(cwd),
            capture_output=True,
            text=False,
            timeout=timeout,
            check=False,
        )
        return CommandResult(
            argv=argv,
            returncode=proc.returncode,
            stdout=proc.stdout,
            stderr=proc.stderr,
            timed_out=False,
        )
    except subprocess.TimeoutExpired as exc:
        return CommandResult(
            argv=argv,
            returncode=124,
            stdout=exc.stdout or b"",
            stderr=exc.stderr or b"",
            timed_out=True,
        )


def find_cimple_binary(repo_root: Path, explicit: Optional[str]) -> Path:
    if explicit:
        candidate = Path(explicit)
        if not candidate.is_absolute():
            candidate = repo_root / candidate
        if candidate.exists():
            return candidate
        raise FileNotFoundError(f"--cimple path does not exist: {candidate}")

    exe_name = "cimple.exe" if IS_WINDOWS else "cimple"
    candidates = [
        repo_root / "build" / "tools" / "cimple_cli" / "Debug" / exe_name,
        repo_root / "build" / "tools" / "cimple_cli" / exe_name,
        repo_root / "build" / "tools" / "cimple_cli" / "Release" / exe_name,
        repo_root / "build" / "tools" / "cimple_cli" / "RelWithDebInfo" / exe_name,
    ]

    for c in candidates:
        if c.exists():
            return c

    in_path = shutil.which("cimple")
    if in_path:
        return Path(in_path)

    raise FileNotFoundError(
        "Could not find cimple binary. Pass --cimple <path> or build tools/cimple_cli first."
    )


def discover_tests(tests_dir: Path, pattern: str) -> list[Path]:
    return sorted(p for p in tests_dir.glob(pattern) if p.is_file())


def expected_exe_path(source_file: Path) -> Path:
    if IS_WINDOWS:
        return source_file.with_suffix(".exe")
    return source_file.with_suffix("")


def print_failure_details(title: str, result: CommandResult) -> None:
    print(f"    {title}: return code {result.returncode}")
    if result.timed_out:
        print("    reason: timed out")

    if result.stdout:
        print("    stdout:")
        for line in decode(result.stdout).splitlines():
            print(f"      {line}")
    if result.stderr:
        print("    stderr:")
        for line in decode(result.stderr).splitlines():
            print(f"      {line}")


def unified_output_diff(expected: bytes, actual: bytes, test_name: str) -> str:
    expected_text = decode(expected).splitlines(keepends=True)
    actual_text = decode(actual).splitlines(keepends=True)
    diff = difflib.unified_diff(
        expected_text,
        actual_text,
        fromfile=f"{test_name} evaluator",
        tofile=f"{test_name} native",
    )
    return "".join(diff)


def parse_args(argv: Optional[Iterable[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run CIMPLE oracle tests.")
    parser.add_argument(
        "--cimple",
        help="Path to cimple executable. If omitted, common build paths are searched.",
    )
    parser.add_argument("--tests-dir", default="tests", help="Directory with .cimp tests.")
    parser.add_argument("--pattern", default="*.cimp", help="Glob pattern inside tests dir.")
    parser.add_argument("--timeout", type=float, default=20.0, help="Timeout per command in seconds.")
    parser.add_argument(
        "--strict-native",
        action="store_true",
        help="Fail if native backend is unavailable (instead of skipping).",
    )
    parser.add_argument(
        "--keep-exe",
        action="store_true",
        help="Keep generated test executables instead of deleting stale ones before build.",
    )
    parser.add_argument(
        "--stop-on-fail",
        action="store_true",
        help="Stop on first mismatch/error.",
    )
    return parser.parse_args(argv)


def main(argv: Optional[Iterable[str]] = None) -> int:
    args = parse_args(argv)

    repo_root = Path(__file__).resolve().parent
    tests_dir = (repo_root / args.tests_dir).resolve()

    try:
        cimple = find_cimple_binary(repo_root, args.cimple)
    except FileNotFoundError as exc:
        print(f"ERROR: {exc}")
        return 2

    tests = discover_tests(tests_dir, args.pattern)
    if not tests:
        print(f"No tests found in {tests_dir} matching {args.pattern}")
        return 2

    print(f"Using cimple: {cimple}")
    print(f"Discovered {len(tests)} test(s) in {tests_dir}")

    passed = 0
    failed = 0
    skipped = 0
    backend_disabled = False
    backend_disabled_reason = ""

    for test_file in tests:
        rel_name = test_file.relative_to(repo_root)
        print(f"\n[TEST] {rel_name}")

        eval_res = run_command([str(cimple), "run", str(test_file)], repo_root, args.timeout)
        if eval_res.returncode != 0:
            failed += 1
            print("  FAIL: evaluator command failed")
            print_failure_details("evaluator", eval_res)
            if args.stop_on_fail:
                break
            continue

        if backend_disabled:
            skipped += 1
            print(f"  SKIP: native backend unavailable ({backend_disabled_reason})")
            continue

        exe_path = expected_exe_path(test_file)
        if exe_path.exists() and not args.keep_exe:
            exe_path.unlink()

        build_res = run_command([str(cimple), "build", str(test_file)], repo_root, args.timeout)
        combined_build_output = build_res.stdout + build_res.stderr

        if b"LLVM backend not enabled" in combined_build_output:
            backend_disabled = True
            backend_disabled_reason = "LLVM backend not enabled in current cimple binary"
            skipped += 1
            print(f"  SKIP: {backend_disabled_reason}")
            continue

        if build_res.returncode != 0:
            failed += 1
            print("  FAIL: build command failed")
            print_failure_details("build", build_res)
            if args.stop_on_fail:
                break
            continue

        if not exe_path.exists():
            failed += 1
            print(f"  FAIL: build did not produce executable: {exe_path}")
            if build_res.stdout or build_res.stderr:
                print_failure_details("build", build_res)
            if args.stop_on_fail:
                break
            continue

        native_res = run_command([str(exe_path)], repo_root, args.timeout)
        if native_res.returncode != 0:
            failed += 1
            print("  FAIL: compiled executable failed")
            print_failure_details("native", native_res)
            if args.stop_on_fail:
                break
            continue

        if eval_res.stdout != native_res.stdout:
            failed += 1
            print("  FAIL: output mismatch")
            diff = unified_output_diff(eval_res.stdout, native_res.stdout, str(rel_name))
            if diff:
                for line in diff.rstrip("\n").splitlines():
                    print(f"    {line}")
            else:
                print("    outputs differ at byte level (non-text diff)")
                print(f"    evaluator bytes: {list(eval_res.stdout)}")
                print(f"    native bytes:    {list(native_res.stdout)}")
            if args.stop_on_fail:
                break
            continue

        passed += 1
        print("  PASS")

    print("\n=== Oracle Summary ===")
    print(f"pass={passed} fail={failed} skip={skipped} total={len(tests)}")

    if failed > 0:
        return 1
    if args.strict_native and skipped > 0:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
