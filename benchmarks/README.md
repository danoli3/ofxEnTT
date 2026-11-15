# CPU benchmarks

Build inside an openFrameworks tree:

```sh
cmake -S benchmarks -B benchmarks/build -DCMAKE_BUILD_TYPE=Release
cmake --build benchmarks/build -j 6
benchmarks/bin/ofxentt_benchmarks.app/Contents/MacOS/ofxentt_benchmarks # macOS
# benchmarks/bin/ofxentt_benchmarks                               # Linux
```

The executable compares frozen reference implementations with the new paths in
the same optimized binary. Results are medians of 11 alternating samples, each
containing 20 frames, after three warmup frames. Checksums consume the outputs.
Scene generation uses seed 42; correctness is checked before timing. Times cover
CPU work only, including checksums and the allocations made by each path. No GL
context or GPU drawing is included. Negative percentages mean less elapsed time.

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
