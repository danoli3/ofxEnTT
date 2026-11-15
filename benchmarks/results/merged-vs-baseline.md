# All merged optimizations versus the original addon

Measured 2026-11-15 on Apple M1 Max (10 physical cores, 64 GiB), macOS 26.6.2,
Apple Clang 21.0.0, ARM64 Release (`-O3 -DNDEBUG`, C++23, no LTO).

Before: `da60c3bc8b321469433fd2215978310fa36912d2`.
After: `76b3900994a753d3e323d26c1475acca73a4793f`.
Both revisions use the same installed, uninstrumented openFrameworks core.

## Complete CPU update and render preparation

Identical moving scenes with 32 mesh batches; includes World updates, collector
work, retained/rebuilt buffers and all per-instance matrix/color packing:

| Scene | Before | Merged defaults | Less elapsed CPU time |
|---|---:|---:|---:|
| 20k linear movers | 1.536 ms | 0.844 ms | 45.1% |
| 20k rotating movers | 1.765 ms | 1.132 ms | 35.9% |
| 200k linear movers | 20.298 ms | 10.022 ms | 50.6% |
| 200k rotating movers | 24.432 ms | 13.406 ms | 45.1% |

With the owning group and four total threads enabled, the 200k rotating scene
measured 10.876 ms: 55.5% less than the original baseline, or about 18.9% less than
the merged default path. Cheap linear motion did not consistently benefit from
workers. These are CPU-stage timings, not GPU or FPS results.

## Other workloads and tradeoffs

- Renderer collection/packing alone used roughly 26–48% less time in the four
  20k/200k one-mesh/32-mesh cases.
- Static chains of eight measured 0.857 ms before, 0.636 ms with merged defaults,
  or 0.502 ms with incremental snapshots enabled. On fully moving hierarchies,
  snapshots cost more than the merged full-update path.
- The 31-sample bulk follow-up measured 56–63% less kernel time for sequential SoA
  versus the original AoS particle arithmetic at 200k/2M particles. These are
  specialized arrays, not the normal ECS default path. Parallel bulk timings
  varied more; the follow-up preserves all samples and spread rather than
  claiming uniform scaling.
- Copying SoA results back into ECS still added cost relative to the merged
  ordinary-motion path. Its improvement over the original addon also includes
  the cheaper world-transform update; it is not a scatter-only speedup.

## Method and reproduction

[Run the public comparison](../revisions/README.md). The main dataset contains
13 samples per configuration after four warmup frames, with 30 frames/sample at
20k, 10 at 200k, or 5 at 2M. Separate persistent executables are measured in
shuffled serial order (seed 20261115); idle configurations wait for input.
Validation and setup are outside the timed region. The 46 comparisons checked
132,760,000 float values and observed zero difference in this run.

- [Raw primary results](merged-vs-baseline.json).
- [31-sample bulk follow-up](merged-vs-baseline-bulk-followup.json).

The recorded source hashes describe the original measurement harness. The public
launcher generalizes paths, machine metadata and build configuration; the timed
workloads are unchanged, while its command protocol also accepts paths with
spaces. New runs record their own hashes and resolved commits.

The local installed core already bundles FreeImage, so both measured executables
omitted its duplicate standalone link. The public build leaves this workaround
off unless explicitly requested. No sanitizer instrumentation or CPU-specific
intrinsic/alignment flags were used. These measurements cover this hardware and
these scenes; per-feature percentages cannot be added to predict total speedup.
