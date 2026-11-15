# Run the original-versus-merged comparison

From the addon root:

```sh
python3 benchmarks/revisions/run.py
```

This is a standalone CPU benchmark, not a graphical openFrameworks application.
It extracts the two frozen revisions into an ignored build directory, builds both
against the same openFrameworks core, runs 13 interleaved samples per comparison,
checks the outputs, and saves JSON results. Your checkout is not switched or
modified. There are 20 scenes and 46 comparisons, including optional settings.

Requires Python 3.10+ with `tarfile.data_filter` (current security-patched Python),
Git with both revisions available locally, CMake 3.22+, and
an openFrameworks tree with its CMake build support and dependencies installed.
Commands are validated on macOS; CI also exercises the Linux build. For shallow
clones, first fetch the historical commits (`git fetch --unshallow` for a shallow
checkout). A source archive without Git history can run the per-feature example
but cannot extract these historical revisions.

## Choose paths and a smaller run

The openFrameworks root defaults to the tree containing this addon. Override it
when needed; all generated files remain under the chosen build directory:

```sh
python3 benchmarks/revisions/run.py --of-root /path/to/openFrameworks

# Complete CPU updates + render preparation for 20k/200k moving instances:
python3 benchmarks/revisions/run.py --filter pipeline

# Quick build/correctness smoke check; use 13+ samples for recorded timings:
python3 benchmarks/revisions/run.py --filter pipeline --samples 3

# Repeat bulk kernels with more samples, reusing the compiled binaries:
python3 benchmarks/revisions/run.py --skip-build --filter bulk-large --samples 31 \
  --output bulk-followup.json
```

Other filters: `motion`, `transforms`, `render`, `bulk`, `scatter`, and `all`.
Use `--help` for all options. `--build-dir` defaults to
`benchmarks/build/revisions`, `--jobs` limits compilation parallelism, and
`--output` is relative to the build directory unless absolute. Build directories
with spaces are supported. Large binary state dumps are removed after comparison;
use `--keep-state` to retain them for inspection.

The default comparison pins these commits:

- Before: `da60c3bc8b321469433fd2215978310fa36912d2`.
- Merged: `76b3900994a753d3e323d26c1475acca73a4793f`.

`--before` and `--after` accept other locally available revisions. The older side
must expose the original World/RenderSystem API; the newer side must expose the
merged optional APIs used in `compare.cpp`. `--skip-build` rejects a build cached
for different revisions. Rerun without it when changing sources/build options.

For a custom prebuilt core that already bundles FreeImage, add
`--core-bundles-freeimage`. This explicit local workaround omits the standalone
FreeImage archive identically in both builds. It is off by default; do not enable
it for a normal core that still needs that archive.

## What each scene measures

| Scene/filter | Timed work |
|---|---|
| `pipeline` | Complete `World::update()` plus renderer CPU collection and instance matrix/color packing for 32 meshes |
| `motion` | Complete World updates for linear, rotating, or hierarchical movers |
| `transforms` | Static roots or chains of eight; default updates versus optional snapshots |
| `render` | Actual batch collection and instance packing for one or 32 meshes; no simulation |
| `scatter` | Complete World updates, with an optional SoA integration/scatter path |
| `bulk` / `bulk-large` | Original AoS particle arithmetic versus SoA kernels, optionally with workers; no ECS scatter |

The reference is a separately compiled source snapshot, not an imitation of the
old implementation. The renderer's small packing loop is reproduced in the
harness, while each revision's actual collector is called. The merged collector
retains buffers exactly as the renderer does during a draw pass.

The runner uses separate persistent processes. It waits for one measured process
at a time, shuffling their order with seed 20261115. Every configuration warms up
for four frames; each sample contains 30 frames at 20k entities, 10 at 200k, or 5
at 2M particles. JSON includes raw samples, medians, spread, resolved commits,
machine metadata, an actual compiler command, source hashes, and validation counts.

After timing, every final local/world transform, packed matrix/color, or bulk
position is compared in canonical order. The validator permits an error of
`1e-4 * max(1, abs(reference))` and rejects non-finite values. The recorded M1 Max
run happened to match bit-for-bit across all 46 comparisons.

Scene creation, process/thread startup, first allocations and validation are
excluded from timing. GPU uploads, draw calls, and FPS are not measured. Run on an
otherwise quiet machine, compare multiple samples, and inspect regressions as
well as gains. Optional paths can lose on cheap or frequently changing workloads.
