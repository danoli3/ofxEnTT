# CPU benchmarks

Two runnable examples are included:

- **Per-feature:** the C++ executable below compares individual algorithms and
  optional paths in one optimized binary.
- **Whole revision:** [the revision launcher](revisions/README.md) builds the
  original and merged addon sources into separate executables and compares full
  CPU frames, updates, renderer preparation, and bulk particle kernels. This is
  the source of the [README results](results/merged-vs-baseline.md).

## Build and run the per-feature example

From the addon root, inside an openFrameworks tree with CMake support:

```sh
cmake -S benchmarks -B benchmarks/build -DCMAKE_BUILD_TYPE=Release
cmake --build benchmarks/build -j 6
benchmarks/bin/ofxentt_benchmarks.app/Contents/MacOS/ofxentt_benchmarks # macOS
# benchmarks/bin/ofxentt_benchmarks                               # Linux
```

Add `-DOF_ROOT=/path/to/openFrameworks` when configuring if needed. The addon must
be installed as `ofxEnTT` in that tree's `addons` directory. Requires CMake 3.22+
and the usual openFrameworks dependencies. Run a Release build for timings.

For make-based openFrameworks installations, `benchmarks/addons.make` is included:
copy the platform's `Makefile` and `config.make` templates into `benchmarks/`, then
run `make -C benchmarks Release`. On macOS the executable is
`benchmarks/bin/benchmarks.app/Contents/MacOS/benchmarks`; on Linux it is
`benchmarks/bin/benchmarks`. This runs the same `src/main.cpp` benchmark.

If a custom prebuilt core already bundles FreeImage, the CMake examples accept
`-DOFXENTT_CORE_BUNDLES_FREEIMAGE=ON` to avoid linking it twice. This is off by
default and was needed only for the local installed core used for the recorded
measurements. Normal builds should leave it off.

The executable compares frozen reference implementations with the new paths in
the same optimized binary. Results are medians of 11 alternating samples, each
containing 20 frames, after three warmup frames. Checksums consume the outputs.
Scene generation uses seed 42; correctness is checked before timing. Times cover
CPU work only, including checksums and the allocations made by each path. No GL
context or GPU drawing is included. Negative percentages mean less elapsed time.

Individual sections below describe exceptions to the default sample size. These
per-feature measurements use a different method from the separate-revision
comparison; do not add their percentages or compare the absolute times directly.

## Direct transforms

20,000 roots and 20,000 entities in chains of eight, with rotations and signed,
nonuniform scales. The reference uses `translate * rotation * scale` and multiplies
roots by an identity matrix. Both paths emit the same registry notifications.
Every resulting world matrix is checked with a relative tolerance of 1e-4.

Measurements and machine details are recorded in the corresponding PR.

## Temporary memory and render preparation

The render comparison uses 20,000 entities with one or 32 meshes. Every seventh
renderer is removed and reinserted before timing so the two component pools do
not accidentally have matching order. The reference collects entity IDs into
fresh vectors and an unordered map, then looks up matrix/color data to pack it.
The new path uses the renderer's actual internal CPU collector, a retained batch
pool and retained lookup entries, and component pointers valid for the current draw. Both pack
all matrix columns and colors; every packed value contributes to the checksum.
The buffers for packed GPU inputs are retained on both sides, as before this PR.
Roots and expired-entity scratch vectors also retain capacity, but are not timed
separately. No GPU work or GL context is used.

## Optional owning group

The owning-group comparison uses the same built-in integration helper with an
EnTT view versus an owning group. It covers 20k and 200k movers, with either zero
angular velocity or quaternion integration. Every seventh Velocity is removed and
reinserted before group creation. The group is created once before timing;
creation/reordering cost is excluded. 20k cases use 20 frames per sample and 200k
cases use 5. All positions and orientations are compared by entity after timing.
The inline checksum adds the same per-entity arithmetic to both paths. This
measures motion only, excluding world-transform propagation, rendering, and setup.

## Incremental transforms

20k roots or chains of eight, with 0%, 1%, or 100% of local positions edited each
frame. The comparison is full updates versus the opt-in snapshot cache, both with
the optimized TRS construction and retained root scratch buffer. Edits, hierarchy
visits, cache comparisons/pruning, and checksums are timed. Warmup builds the cache;
its first-frame allocation/setup and extra memory are excluded from timing. Every
world matrix is checked against full updates after timing. Descendants of edited
parents also update, so 1% edited local transforms need not mean 1% world updates.

## SIMD-friendly bulk linear motion

Kernel-only comparisons use an array of `{glm::vec3 position, velocity}` versus
`LinearMotionBatch`, with 20k, 200k and 2M particles. Both perform the same three
linear updates; positions on all axes are verified afterward (absolute tolerance
1e-4). A first/last position checksum is timed, not a full serial reduction that
could obscure vectorized kernel time. 20k cases use 20 frames/sample; larger cases
use 5. Setup/reserve/conversion costs are excluded.

Separate 20k/200k comparisons include both bulk integration and its serial scatter
to ECS Transforms, versus built-in view-based motion with zero angular velocities.
All local positions are checked by entity. Both exclude world-matrix propagation
and drawing. No kernel-only result should be read as an end-to-end ECS speedup.

## Optional parallel CPU updates

Four total threads (three persistent workers plus the caller) versus one. World
comparisons include complete update calls: pointer gathering, scheduling, joins,
expiry scans, command draining, and serial world-transform propagation. Both worlds
use the same optional owning group. There are 20k/200k movers with zero or nonzero
angular velocity. Every local position and orientation is checked afterward.

Separate SoA comparisons cover 20k/200k/2M particles, including scheduling and joins
but excluding ECS scatter. Inputs below 2 × 16384 items use the caller; this includes
the 20k case. Initial thread creation and first buffer allocations are excluded by
warmup. Samples use 20 frames for 20k and 5 for larger inputs. The result depends on
core count, scheduling, memory bandwidth and whether the arithmetic is expensive
enough to amortize gathering. There is no parallel GL work or hierarchy scheduler.

### Worker stress check with ThreadSanitizer

This standalone check does not link openFrameworks. It runs 700 uneven jobs,
changes pool sizes, and checks exception recovery:

```sh
clang++ -std=c++20 -O1 -g -fsanitize=thread -fno-omit-frame-pointer -I src \
  src/ofxEnTTParallel.cpp benchmarks/tests/parallel_stress.cpp -o /tmp/ofxentt-parallel-stress
/tmp/ofxentt-parallel-stress
```

To check the threadless fallback on a native compiler, build the same two sources
with `-D__EMSCRIPTEN__` and without `-fsanitize=thread`. This exercises the fallback
selection, not a full WebAssembly build.

For the local mixed-ASan/prebuilt-core limitation and a core-only reproducer, see
[parallel-motion results](results/parallel-motion.md#correctness-and-sanitizer-evidence).
Do not interpret a qualified run with container annotations disabled as an
unrestricted sanitizer pass. A consistently ASan-built core is the preferred setup.
