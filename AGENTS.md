# Repository instructions

- When something in this repo trips you up, add one concise, factual line under Gotchas explaining the issue and the working fix; update an existing line instead of duplicating it.
- Add no Codex attribution to commits, PR descriptions, issues, or comments, including `Co-Authored-By: Codex` trailers or "Generated with Codex" text.

## Gotchas

- The local prebuilt macOS openFrameworks core already bundles FreeImage; for duplicate FreeImage symbols, use the benchmark CMake option `OFXENTT_CORE_BUNDLES_FREEIMAGE=ON` or revision runner flag `--core-bundles-freeimage`, leaving this workaround off for normal cores.
- Mixing ASan addon code with the local non-ASan prebuilt core can report vector container-overflow during renderer setup; prefer a fully ASan-built core, or qualify runs using `ASAN_OPTIONS=detect_container_overflow=0` rather than claiming unrestricted sanitizer success.
- Zero numerical difference does not prove byte-for-byte equality: positive and negative zero can have different bits, and the benchmark validator compares numeric values.
- The revision benchmark needs both historical commits locally; use `git fetch --unshallow` for shallow clones before running it, and use a patched Python 3.10+ that provides `tarfile.data_filter`.
- `RenderSystem::collectBatches` only sees entities that already have `WorldTransform`; call `world.update()` (or `transforms.update(registry)`) before `collectBatches` or the result is empty and `batches[0]` is UB.
- `example_tests` scene headers are included via the relative `../../example_aquarium/src/aquariumScene.h` and `../../example_mineent/src/mineentScene.h` paths; keep the scene code header-only so tests stay a single translation unit.

## Progress (2026-11-26)

### Shipped
- PR #8 — subtree destroy O(N²)→O(N) via `std::move` of the children vector before recursing; transform cache skips its stale-entry sweep when `visited == cache.size()`.
- PR #9 — `example_aquarium` (schooling fish, 3 species, 538 entities, instanced teardrop mesh per species) and `example_mineent` (fBm voxel terrain + trees, one shared cube mesh, radius-24 default).
- PR #9 — `testAquariumScene` and `testMineentScene` in `example_tests`; 14 new assertions covering counts, component coverage, determinism, swim motion, and single-batch rendering. 124/124 passing.
- PR #10 — `docs/images/example_aquarium.jpg` and `docs/images/example_mineent.jpg`; both embedded in the `README.md` example table.
- PR #11 — `benchmarks/src/WorldBench.h` with `subtreeDestroyBenchmark` and `transformCacheCleanupBenchmark`; README gains a CPU-performance subsection with the results table.
- Open on `feat/events-spatial-culling` — cross-system event bus (`World::events` is an `entt::dispatcher`), opt-in `SpatialHash` broadphase (`LocalBounds` + `SpatialMember` + `queryRadius`), opt-in `CullingSystem` (`LocalBounds` + `Culled` tag, excluded by `RenderSystem`). All three are off unless a component or tag is present, so existing scenes pay nothing. Tests: 164/164 passing (+40).

### Measured wins (M1 Max, Apple Clang 21, -O3 -DNDEBUG, 11-sample medians)
| Scene | Before | After | Delta |
| --- | --- | --- | --- |
| subtree destroy / 500 siblings | 106 μs | 50 μs | -52.8% |
| subtree destroy / 2 000 siblings | 902 μs | 171 μs | -81.0% |
| subtree destroy / 8 000 siblings | 12.33 ms | 0.74 ms | -94.0% |
| transform cache / 20 k stable | 6.65 μs | 0 μs | ~-100% |
| transform cache / 200 k stable | 66.6 μs | 0 μs | ~-100% |

## Roadmap / enhancements

Scoped against what entt and similar ECS engines (Hazel, Lumos, the entt examples repo, voxel renderers) do well that `ofxEnTT` doesn't yet. Each item names the component(s) it would introduce and a test or benchmark that would prove the win.

### Rendering throughput
- ~~Frustum culling system.~~ Shipped on `feat/events-spatial-culling`: `LocalBounds` + `Culled` tag, opt-in `CullingSystem::update(registry, camera)`. Still TODO: wire it into `example_mineent` at radius 48 and record the draw-call reduction.
- **Material/shader-aware batching.** Extend `RenderBatch` to key on (mesh, shader, material uniforms) rather than mesh alone, so instanced colour lanes can share a draw. Lets the aquarium collapse three species into one batch if the shader reads colour from an instance buffer.
- **Groups instead of views for the hot path.** `entt::group<Transform, WorldTransform>` owns both pools and iterates ~2× faster than a view. Try it for `TransformSystem::update` and add a group-vs-view row to `benchmarks/src/main.cpp`.
- **LOD component.** `LOD { std::vector<std::pair<float, std::shared_ptr<ofMesh>>> tiers; }`; a system swaps `RenderMesh` based on camera distance. Useful for voxel scenes with distant chunks.

### Spatial queries
- ~~Uniform grid / hash broadphase.~~ Shipped on `feat/events-spatial-culling`: `SpatialHash` with `setCellSize`, `rebuild`, `queryBounds`, `queryRadius`; opt-in via `SpatialMember` + `LocalBounds`. Still TODO: ray query, and rebuilding the aquarium `swim` on top of it for true boids.
- **Picking.** `MouseRay` → iterate `LocalBounds` through the grid, intersect in world space, return closest entity. One test asserting the cube under the cursor is reported.

### Reactivity and change detection
- **Observer-driven dirty flags.** `entt::observer` on `on_update<Transform>()` sets a `TransformDirty` tag; `TransformSystem` iterates only dirty subtrees, eliminating the per-entity `lastSeen` map entirely. Simpler and probably faster than the current incremental cache.
- ~~Event dispatcher.~~ Shipped on `feat/events-spatial-culling`: `World::events` is an `entt::dispatcher`.

### Scene lifecycle
- **Snapshot / serialize.** Wrap `entt::snapshot`/`snapshot_loader` behind `World::save(ostream)` / `World::load(istream)` with a registered component list. First target: save/restore a mineent scene so world-gen cost moves off the critical path on rebuild.
- **Prefab component.** `Prefab { std::string id; }` resolved through an `AssetRegistry`; `World::instantiate(prefab, transform)` stamps a saved subtree via `entt::registry::storage().each`.

### Animation and gameplay
- **Tween/animation component.** `Tween<Transform> { field, from, to, duration, ease }`; one `AnimationSystem` advances all tweens. Powers the mineent spin and lets the aquarium cross-fade colours.
- **Boids system.** Proper separation/alignment/cohesion built on the spatial hash, replacing the ad-hoc wander in `aquariumScene::swim`. Becomes a reference example for neighbour-query ECS code.

### Parallelism
- **Chunked view iteration.** The entt docs show `std::for_each(std::execution::par_unseq, view.begin(), view.end(), …)` on groups; try it for `TransformSystem` leaf updates on scenes with ≥50k roots and add a benchmark row.

### Tooling
- **Named systems with dependencies.** `addSystem(name, deps, fn)` with a tiny DAG; `World::update` topologically sorts once. Removes order bugs like the mineent `collectBatches`-before-`update` crash.
- **Registry introspection HUD.** `ofxEnTTDebugDraw` listing component counts, dirty-subtree percentages, and top-N entities by child count. Feeds into future editor integration.
