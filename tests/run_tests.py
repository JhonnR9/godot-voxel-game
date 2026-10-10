#!/usr/bin/env python3
"""Discover and run project tests with live logs and isolated persistent data."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import time

ROOT = Path(__file__).resolve().parents[1]
ERROR = re.compile(r"(?:^|\n)(?:SCRIPT ERROR:|ERROR:)|\b[1-9][0-9]* failures\b", re.IGNORECASE)
ANSI_ESCAPE = re.compile(r"\x1b\[[0-9;]*m")


@dataclass(frozen=True)
class Test:
    name: str
    path: Path
    suite: str
    renderer: bool = False


def discover(include_benchmarks: bool) -> list[Test]:
    tests = []
    for suite, directory, extension in [("python", ROOT / "tests", "py"),
                                        ("native", ROOT / "tests", "cpp"), ("godot", ROOT / "project/tests", "gd")]:
        paths = set(directory.glob(f"*_test.{extension}"))
        if include_benchmarks:
            paths.update(directory.glob(f"*_benchmark.{extension}"))
        for path in sorted(paths):
            tests.append(Test(f"{suite}/{path.stem}", path, suite, "TEST_REQUIRES_RENDERER" in path.read_text()))
    return tests


def run_process(command: list[str], env: dict[str, str], log: Path, timeout: float) -> tuple[str, float, str]:
    started = time.monotonic()
    print(f"    $ {shlex.join(command)}", flush=True)
    with log.open("w", encoding="utf-8") as output:
        output.write(shlex.join(command) + "\n")
        output.flush()
        process = subprocess.Popen(command, cwd=ROOT, env=env, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, text=True, errors="replace",
                                   start_new_session=os.name == "posix")
        lines: list[str] = []

        def stream() -> None:
            assert process.stdout is not None
            with process.stdout:
                for line in process.stdout:
                    output.write(line)
                    output.flush()
                    lines.append(line)
                    print("    " + line.rstrip(), flush=True)

        reader = threading.Thread(target=stream, daemon=True)
        reader.start()
        status = "PASS"
        reason = ""
        try:
            code = process.wait(timeout=timeout)
            if code:
                status, reason = "FAIL", f"exit code {code}"
        except (subprocess.TimeoutExpired, KeyboardInterrupt) as exc:
            if os.name == "posix":
                os.killpg(process.pid, signal.SIGKILL)
            else:
                process.kill()
            process.wait()
            if isinstance(exc, KeyboardInterrupt):
                reader.join()
                raise
            status, reason = "TIMEOUT", f"exceeded {timeout:g}s"
        reader.join()
        if status == "PASS" and ERROR.search(ANSI_ESCAPE.sub("", "".join(lines))):
            status, reason = "FAIL", "error reported in process output"
    return status, time.monotonic() - started, reason


def registry_snapshot(path: Path) -> None:
    """Export JSON data to a simple native-test fixture; never modify game assets."""
    data = json.loads((ROOT / "project/data/biome_registry.json").read_text())
    blocks = json.loads((ROOT / "project/data/block_registry.generated.json").read_text())
    source = json.loads((ROOT / "project/data/block_registry.json").read_text())
    source_ids = {b["name"]: b["id"] for b in source["blocks"]}
    ids = {b["name"]: b["id"] for b in blocks["blocks"]}
    if source_ids != ids:
        raise ValueError("Block source/generated IDs differ; rebuild block assets and the extension first.")
    rows = []
    world = data["world"]
    rows.append(f"world {world['sea_level']} {world.get('wet_coast_offset', -8)} {world.get('dry_coast_offset', 1)}")
    for b in blocks["blocks"]:
        rows.append(f"block {b['id']} {b['name']} {b['flag_mask']}")
    for biome in data["biomes"]:
        name = biome["name"]
        selection = biome.get("selection", {})
        rows.append(f"biome {biome['id']} {name} {biome.get('rarity', 1)} "
                    f"{selection.get('climate_max', 1)} {biome.get('surface_fill', 'water')}")
        v = biome.get("vegetation", {})
        if not v:
            continue
        values = [v.get(k, default) for k, default in
                  [("patch_size", 12), ("patch_chance", 1000), ("cluster_radius", 0),
                   ("coverage_min", 0), ("coverage_max", v.get("coverage_min", 0)),
                   ("flowers_min", 0), ("flowers_max", v.get("flowers_min", 0))]]
        rows.append(f"profile {name} " + " ".join(map(str, values)))
        for kind in ["plants", "flowers"]:
            for item in v.get(kind, []):
                rows.append(f"plant {name} {kind} {ids[item['block']]} {item.get('weight', 1)} "
                            f"{item.get('min_height', 1)} {item.get('max_height', item.get('min_height', 1))}")
    path.write_text("\n".join(rows) + "\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--suite", choices=["all", "godot", "native", "python"], default="all")
    parser.add_argument("--filter", default="", help="Run names containing this substring")
    parser.add_argument("--list", action="store_true", help="List discovered tests without executing")
    parser.add_argument("--include-benchmarks", action="store_true")
    parser.add_argument("--godot", default=os.environ.get("GODOT", "godot"))
    parser.add_argument("--cxx", default=os.environ.get("CXX", "c++"))
    parser.add_argument("--timeout", type=float, default=90, help="Timeout per process in seconds")
    parser.add_argument("--seeds", default="42,1234,2026", help="Comma-separated deterministic sampling seeds")
    parser.add_argument("--render-tests", choices=["auto", "on", "off"], default="auto")
    parser.add_argument("--require-all", action="store_true", help="Return failure if any selected test is skipped")
    parser.add_argument("--skip-import", action="store_true", help="Use an already imported project")
    parser.add_argument("--output", type=Path, help="New output directory for logs, fixtures, and report")
    args = parser.parse_args()
    tests = [t for t in discover(args.include_benchmarks)
             if (args.suite == "all" or args.suite == t.suite) and args.filter in t.name]
    if not tests:
        parser.error("No tests match the selection.")
    if args.list:
        for t in tests:
            print(t.name + (" [renderer required]" if t.renderer else ""))
        return 0
    if args.timeout <= 0:
        parser.error("--timeout must be positive.")
    try:
        seeds = [int(s.strip()) for s in args.seeds.split(",")]
        if not seeds or any(s < -(2**31) or s > 2**31 - 7 for s in seeds):
            raise ValueError
    except ValueError:
        parser.error("--seeds must contain signed 32-bit integers with room for noise offsets.")
    executables = {}
    for suite, executable in [("godot", args.godot), ("native", args.cxx)]:
        if any(t.suite == suite for t in tests):
            found = shutil.which(executable)
            if not found:
                parser.error(f"Missing {suite} executable: {executable}")
            executables[suite] = found
    if sys.platform == "darwin" and "godot" in executables:
        parser.error("Godot user-data isolation currently supports Linux and Windows; macOS support is pending.")
    if args.output:
        output = args.output.resolve()
        if output.exists():
            parser.error("--output must be a new directory to prevent reusing test saves.")
        output.mkdir(parents=True)
    else:
        output = Path(tempfile.mkdtemp(prefix="voxelgames-tests-"))
    print(f"Discovered {len(tests)} tests. Artifacts: {output}", flush=True)
    base_env = os.environ.copy()
    base_env.update({"TEST_SEEDS": ",".join(map(str, seeds)), "PYTHONUNBUFFERED": "1"})

    def isolated_env(directory: Path) -> dict[str, str]:
        env = base_env.copy()
        for variable, folder in [("XDG_DATA_HOME", "data"), ("XDG_CONFIG_HOME", "config"),
                                 ("XDG_CACHE_HOME", "cache"), ("APPDATA", "data"),
                                 ("LOCALAPPDATA", "data")]:
            target = directory / folder
            target.mkdir(parents=True, exist_ok=True)
            env[variable] = str(target)
        return env

    results = []
    started = time.monotonic()

    def record(name: str, status: str, duration: float, reason: str, log: Path | None) -> None:
        results.append({"name": name, "status": status, "seconds": round(duration, 3),
                        "reason": reason, "log": str(log) if log else None})
        print(f"[{status}] {name} ({duration:.2f}s)" + (f" — {reason}" if reason else ""), flush=True)
        report = {"created_at": datetime.now(timezone.utc).isoformat(), "seeds": seeds,
                  "seconds": round(time.monotonic() - started, 3), "results": results}
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")

    fixture = output / "registry.txt"
    try:
        registry_snapshot(fixture)
    except (OSError, ValueError, KeyError) as exc:
        record("setup/registry", "FAIL", 0, str(exc), None)
        return 1
    if "godot" in executables and not args.skip_import:
        log = output / "import.log"
        status, seconds, reason = run_process([executables["godot"], "--headless", "--editor", "--path",
                                              str(ROOT / "project"), "--import"],
                                             isolated_env(output / "import"), log, max(120, args.timeout))
        record("setup/import", status, seconds, reason, log)
        if status != "PASS":
            print("Import failed; build the extension and check Godot compatibility before retrying.")
            return 1
    renderer_available = args.render_tests == "on" or (args.render_tests == "auto" and
                         (sys.platform != "linux" or bool(os.environ.get("DISPLAY") or os.environ.get("WAYLAND_DISPLAY"))))
    for index, test in enumerate(tests, 1):
        print(f"\n[{index}/{len(tests)}] {test.name}", flush=True)
        folder = output / test.suite / test.path.stem
        folder.mkdir(parents=True)
        env = isolated_env(folder)
        if test.renderer and not renderer_available:
            record(test.name, "SKIP", 0, "requires an active renderer; use --render-tests on with a display", None)
            continue
        compile_seconds = 0.0
        if test.suite == "python":
            command = [sys.executable, str(test.path)]
        elif test.suite == "native":
            binary = folder / (test.path.stem + (".exe" if os.name == "nt" else ""))
            command = [executables["native"], "-std=c++17", "-O2", "-Wall", "-Wextra", "-pedantic",
                       "-I" + str(ROOT / "godot-cpp/include"), "-I" + str(ROOT / "godot-cpp/gen/include"),
                       "-I" + str(ROOT / "godot-cpp/gdextension"), str(test.path), "-o", str(binary)]
            log = folder / "compile.log"
            status, seconds, reason = run_process(command, env, log, args.timeout)
            if status != "PASS":
                record(test.name, status, seconds, "compile: " + reason, log)
                continue
            compile_seconds = seconds
            command = [str(binary), str(fixture), str(seeds[0])]
        else:
            command = [executables["godot"], "--path", str(ROOT / "project")]
            if test.renderer:
                command += ["--rendering-method", "gl_compatibility", "--rendering-driver", "opengl3"]
            else:
                command += ["--headless"]
            command += ["--script", "res://tests/" + test.path.name]
        # The disk test deliberately shares its isolated directory across two processes.
        phases = [("write", []), ("read", ["--", "--read"])] if test.path.stem == "inventory_disk_test" else [("run", [])]
        for phase, extra in phases:
            log = folder / (phase + ".log")
            status, seconds, reason = run_process(command + extra, env, log, args.timeout)
            record(test.name + ("/" + phase if len(phases) > 1 else ""), status, seconds + compile_seconds, reason, log)
            if status != "PASS":
                break
    test_statuses = []
    for test in tests:
        phases = [r["status"] for r in results if r["name"] == test.name or r["name"].startswith(test.name + "/")]
        test_statuses.append(next(s for s in ["FAIL", "TIMEOUT", "SKIP", "PASS"] if s in phases))
    counts = {s: test_statuses.count(s) for s in ["PASS", "FAIL", "TIMEOUT", "SKIP"]}
    report_path = output / "report.json"
    report = json.loads(report_path.read_text())
    report["totals"] = counts
    report["selected_tests"] = len(tests)
    report_path.write_text(json.dumps(report, indent=2) + "\n")
    print("\nSummary: " + ", ".join(f"{count} {status}" for status, count in counts.items()), flush=True)
    print(f"Logs and JSON report: {output}", flush=True)
    return int(bool(counts["FAIL"] or counts["TIMEOUT"] or (args.require_all and counts["SKIP"])))


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\nTest run interrupted.", file=sys.stderr)
        sys.exit(130)
