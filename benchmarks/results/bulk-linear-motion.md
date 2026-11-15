# Optional SIMD-friendly linear motion

Apple M1 Max (10 cores, 64 GiB), macOS 26.6.2, Apple Clang 21, ARM64 Release
`-O3 -DNDEBUG`. Method: ../README.md. No GLM layout changes or CPU-only intrinsics.

| Workload | Before | After | Time change |
|---|---:|---:|---:|
| 20k particles, AoS vs SoA kernel | 16.88 us | 6.77 us | -59.9% |
| 200k particles, AoS vs SoA kernel | 195.02 us | 72.64 us | -62.8% |
| 2M particles, AoS vs SoA kernel | 1909.92 us | 841.41 us | -55.9% |
| 20k movers, ECS view vs SoA + ECS scatter | 62.21 us | 85.23 us | +37.0% |
| 200k movers, ECS view vs SoA + ECS scatter | 635.65 us | 853.23 us | +34.2% |

The first three comparisons exclude ECS lookup/scatter and initial conversion;
AoS is a vector of position/velocity vec3 pairs. The last two include the explicit
serial syncTo(World&) scatter, with equivalent ECS view-based integration as the
reference. World-matrix propagation and drawing are excluded throughout. This
specialized storage is useful when data can remain in the bulk arrays; syncing
every item to ECS can cost more than the kernel saves.

Apple Clang `-Rpass=loop-vectorize` confirms all three axis loops vectorize with
width 4, interleave count 4, on ARM64. The same source retains a portable scalar
fallback on other compilers/targets. All final positions match within 1e-4.
98/98 addon checks passed. The temporary prebuilt-core/FreeImage workaround from
 direct-transforms.md applies.
