#!/usr/bin/env python3
"""Build two isolated addon revisions, interleave measurements, and validate outputs."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import queue
import random
import shlex
import statistics
import subprocess
import sys
import tarfile
import tempfile
import threading
import time

SOURCE = Path(__file__).resolve().parent
REPO = SOURCE.parent.parent
BEFORE = "da60c3bc8b321469433fd2215978310fa36912d2"
AFTER = "76b3900994a753d3e323d26c1475acca73a4793f"
CASES = [
    ("static_roots", 20000, 0, ["default", "incremental"]),
    ("static_hierarchy", 20000, 0, ["default", "incremental"]),
    ("linear", 20000, 0, ["default", "group", "parallel"]),
    ("rotating", 20000, 0, ["default", "group", "parallel"]),
    ("linear", 200000, 0, ["default", "group", "parallel"]),
    ("rotating", 200000, 0, ["default", "group", "parallel"]),
    ("hierarchy", 20000, 0, ["default", "group", "parallel", "incremental"]),
    ("render", 20000, 1, ["default"]),
    ("render", 20000, 32, ["default"]),
    ("render", 200000, 1, ["default"]),
    ("render", 200000, 32, ["default"]),
    ("scatter", 20000, 0, ["default", "bulk-sync"]),
    ("scatter", 200000, 0, ["default", "bulk-sync"]),
    ("frame_linear", 20000, 32, ["default", "group", "parallel"]),
    ("frame_rotating", 20000, 32, ["default", "group", "parallel"]),
    ("frame_linear", 200000, 32, ["default", "group", "parallel"]),
    ("frame_rotating", 200000, 32, ["default", "group", "parallel"]),
    ("bulk", 20000, 0, ["default", "parallel"]),
    ("bulk", 200000, 0, ["default", "parallel"]),
    ("bulk", 2000000, 0, ["default", "parallel"]),
]


def positive(value):
    number = int(value)
    if number < 1:
        raise argparse.ArgumentTypeError("must be positive")
    return number


def snapshot(revision, label, build):
    try:
        sha = subprocess.check_output(["git", "-C", str(REPO), "rev-parse", "--verify",
            "--end-of-options", revision + "^{commit}"], text=True, stderr=subprocess.PIPE).strip()
    except subprocess.CalledProcessError as error:
        raise RuntimeError(f"Revision {revision} is unavailable locally. Fetch full history first "
            "(git fetch --unshallow for a shallow clone).") from error
    destination = build / "sources" / (label + "-" + sha)
    marker = destination / ".benchmark-revision"
    if not marker.exists():
        destination.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryFile() as archive:
            subprocess.run(["git", "-C", str(REPO), "archive", sha], stdout=archive, check=True)
            archive.seek(0)
            with tarfile.open(fileobj=archive) as files:
                files.extractall(destination, filter="data")
        marker.write_text(sha + "\n")
    return sha, destination


class Process:
    def __init__(self, executable, kind, count, mode, meshes, label, data, timeout):
        self.label, self.timeout = label, timeout
        self.log = data / f"{kind}-{count}-{meshes}-{label}.stderr.log"
        self.stderr = self.log.open("w")
        self.process = subprocess.Popen([str(executable), kind, str(count), mode, str(meshes)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.stderr, text=True, bufsize=1)
        self.lines = queue.Queue()
        threading.Thread(target=self.read_lines, daemon=True).start()
        try:
            self.receive()
        except BaseException:
            self.process.kill()
            self.process.wait()
            self.stderr.close()
            raise
        self.timings = []

    def read_lines(self):
        for line in self.process.stdout:
            self.lines.put(line)
        self.lines.put(None)

    def receive(self):
        deadline = time.monotonic() + self.timeout
        while True:
            try:
                line = self.lines.get(timeout=max(0, deadline - time.monotonic()))
            except queue.Empty as error:
                raise RuntimeError(f"{self.label} timed out; see {self.log}") from error
            if line is None:
                raise RuntimeError(f"{self.label} exited unexpectedly; see {self.log}")
            try:
                return json.loads(line)
            except json.JSONDecodeError:
                self.stderr.write(line)
                self.stderr.flush()

    def request(self, command):
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()
        return self.receive()

    def close(self):
        try:
            if self.process.poll() is None:
                self.process.stdin.write("quit\n")
                self.process.stdin.flush()
            self.process.wait(timeout=15)
        except (BrokenPipeError, subprocess.TimeoutExpired):
            self.process.kill()
            self.process.wait()
        finally:
            self.stderr.close()
        if self.process.returncode:
            raise RuntimeError(f"{self.label} exited with {self.process.returncode}; see {self.log}")


def summarize(values):
    ordered = sorted(values)
    median = statistics.median(ordered)
    return {"median_us": median, "min_us": ordered[0], "max_us": ordered[-1],
        "p10_us": ordered[round(.1 * (len(ordered) - 1))],
        "p90_us": ordered[round(.9 * (len(ordered) - 1))],
        "median_absolute_deviation_us": statistics.median(abs(x - median) for x in ordered),
        "samples_us": values}


def cpu_model():
    if platform.system() == "Darwin":
        result = subprocess.run(["sysctl", "-n", "machdep.cpu.brand_string"], text=True, capture_output=True)
        if result.returncode == 0:
            return result.stdout.strip()
    cpuinfo = Path("/proc/cpuinfo")
    if cpuinfo.exists():
        for line in cpuinfo.read_text().splitlines():
            if line.startswith("model name"):
                return line.split(":", 1)[1].strip()
    return platform.processor() or platform.machine()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--of-root", type=Path, default=REPO.parent.parent)
    parser.add_argument("--build-dir", type=Path, default=REPO / "benchmarks/build/revisions")
    parser.add_argument("--before", default=BEFORE)
    parser.add_argument("--after", default=AFTER)
    parser.add_argument("--samples", type=positive, default=13)
    parser.add_argument("--filter", choices=["all", "pipeline", "motion", "transforms", "render", "bulk", "bulk-large", "scatter"], default="all")
    parser.add_argument("--output", type=Path, default=Path("results.json"))
    parser.add_argument("--jobs", type=positive, default=min(6, os.cpu_count() or 1))
    parser.add_argument("--timeout", type=positive, default=120, help="seconds per sample/setup response")
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--core-bundles-freeimage", action="store_true", help="omit standalone FreeImage when the local prebuilt core already bundles it")
    parser.add_argument("--keep-state", action="store_true", help="retain large validation dumps")
    args = parser.parse_args()
    if sys.version_info < (3, 10) or not hasattr(tarfile, "data_filter"):
        parser.error("Use Python 3.10+ with tarfile.data_filter (update Python for safe snapshot extraction)")
    build = args.build_dir.resolve()
    data = build / "run-data"
    data.mkdir(parents=True, exist_ok=True)
    before_sha, before = snapshot(args.before, "before", build)
    after_sha, after = snapshot(args.after, "merged", build)
    if not args.skip_build:
        subprocess.run(["cmake", "-S", str(SOURCE), "-B", str(build), "-DCMAKE_BUILD_TYPE=Release",
            "-DOF_USE_CCACHE=OFF", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
            "-DOF_ROOT=" + str(args.of_root.resolve()), "-DOFXENTT_BEFORE_DIR=" + str(before),
            "-DOFXENTT_AFTER_DIR=" + str(after),
            "-DOFXENTT_CORE_BUNDLES_FREEIMAGE=" + ("ON" if args.core_bundles_freeimage else "OFF")], check=True)
        subprocess.run(["cmake", "--build", str(build), "--config", "Release", "-j", str(args.jobs)], check=True)
    # Reject stale --skip-build binaries rather than silently comparing wrong revisions.
    cache = (build / "CMakeCache.txt").read_text()
    for key, value in [("OFXENTT_BEFORE_DIR", before), ("OFXENTT_AFTER_DIR", after)]:
        if f"{key}:PATH={value}" not in cache:
            raise RuntimeError("Build does not match the selected revisions. Run without --skip-build.")
    freeimage = "ON" if args.core_bundles_freeimage else "OFF"
    if f"OFXENTT_CORE_BUNDLES_FREEIMAGE:BOOL={freeimage}" not in cache or f"OF_ROOT:PATH={args.of_root.resolve()}" not in cache:
        raise RuntimeError("Build options differ from the cached build. Repeat the same options or run without --skip-build.")
    output = args.output if args.output.is_absolute() else build / args.output
    output.parent.mkdir(parents=True, exist_ok=True)
    commands = json.loads((build / "compile_commands.json").read_text())
    compile_command = commands[0].get("command") or shlex.join(commands[0]["arguments"])
    report = {
        "baseline_revision": before_sha, "merged_revision": after_sha,
        "utc_started": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "cpu": cpu_model(), "logical_cpus": os.cpu_count(), "os": platform.platform(),
        "architecture": platform.machine(), "example_compile_command": compile_command,
        "harness_sha256": {name: hashlib.sha256((SOURCE / name).read_bytes()).hexdigest()
            for name in ["compare.cpp", "validate.cpp", "CMakeLists.txt", "run.py"]},
        "method": {"samples": args.samples, "warmup_frames": 4,
            "frames_per_sample": "30 at 20k, 10 at 200k, 5 at 2M",
            "order": "Separate persistent executables, shuffled serial order per sample (seed 20261115)",
            "validation": "Every final state float; error <= 1e-4 * max(1, abs(reference)); validation outside timing",
            "scope": "Complete World updates; actual renderer CPU collection/packing; frame cases combine both; bulk compares AoS/SoA without ECS scatter",
            "excluded": "Scene/process setup, first allocations/thread creation, validation, GPU work",
            "opt_in": "group = owning group; parallel = group + 4 total threads for World or 4 threads for bulk; incremental = snapshots; bulk-sync = SoA + serial ECS scatter + World update",
            "core_bundles_freeimage": args.core_bundles_freeimage}, "cases": []}
    cases = CASES
    filters = {"pipeline": lambda c: c[0].startswith("frame_"),
        "motion": lambda c: c[0] in ["linear", "rotating", "hierarchy"],
        "transforms": lambda c: c[0].startswith("static_"), "render": lambda c: c[0] == "render",
        "bulk": lambda c: c[0] == "bulk", "bulk-large": lambda c: c[0] == "bulk" and c[1] >= 200000,
        "scatter": lambda c: c[0] == "scatter"}
    if args.filter != "all":
        cases = [case for case in cases if filters[args.filter](case)]
    randomizer = random.Random(20261115)
    suffix = ".exe" if os.name == "nt" else ""
    for kind, count, meshes, modes in cases:
        frames = 30 if count == 20000 else (10 if count == 200000 else 5)
        processes, dumps = [], []
        try:
            processes.append(Process(build / ("bin/before_bench" + suffix), kind, count, "default", meshes, "before", data, args.timeout))
            for mode in modes:
                processes.append(Process(build / ("bin/merged_bench" + suffix), kind, count, mode, meshes, "merged-" + mode, data, args.timeout))
            for sample in range(args.samples):
                order = list(processes)
                randomizer.shuffle(order)
                for process in order:
                    process.timings.append(process.request(f"measure {frames}")["us"])
            before_dump = data / f"{kind}-{count}-{meshes}-before.bin"
            dumps.append(before_dump)
            processes[0].request(f"dump {before_dump}")
            entry = {"scenario": kind, "entities_or_particles": count, "meshes": meshes,
                "frames_per_sample": frames, "before": summarize(processes[0].timings), "merged": {}}
            for process in processes[1:]:
                mode = process.label.removeprefix("merged-")
                state = data / f"{kind}-{count}-{meshes}-{process.label}.bin"
                dumps.append(state)
                process.request(f"dump {state}")
                validation = json.loads(subprocess.check_output([str(build / ("bin/compare_states" + suffix)), str(before_dump), str(state)], text=True))
                result = summarize(process.timings)
                result["time_change_percent"] = 100 * (result["median_us"] / entry["before"]["median_us"] - 1)
                result["speedup"] = entry["before"]["median_us"] / result["median_us"]
                result["validation"] = validation
                entry["merged"][mode] = result
                print(f"{kind:18s} {count:8d} meshes={meshes:2d} {mode:12s} "
                    f"{entry['before']['median_us']:9.2f} -> {result['median_us']:9.2f} us "
                    f"{result['time_change_percent']:+6.1f}%  checked {validation['floats']} floats", flush=True)
            report["cases"].append(entry)
            output.write_text(json.dumps(report, indent=2) + "\n")
        finally:
            close_errors = []
            for process in processes:
                try:
                    process.close()
                except RuntimeError as error:
                    close_errors.append(str(error))
            if not args.keep_state:
                for path in dumps:
                    path.unlink(missing_ok=True)
            if close_errors:
                raise RuntimeError("; ".join(close_errors))
    report["utc_finished"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    output.write_text(json.dumps(report, indent=2) + "\n")
    print(f"Saved {output}", flush=True)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.CalledProcessError, OSError) as error:
        print(f"Benchmark failed: {error}", file=sys.stderr)
        sys.exit(1)
