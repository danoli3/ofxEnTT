# Optional velocity owning group

Apple M1 Max (10 cores, 64 GiB), macOS 26.6.2, Apple Clang 21, ARM64 Release
`-O3 -DNDEBUG`. Method: ../README.md. Group creation is outside the timed region.
Both sides use the same built-in velocity integration helper and checksum.

| Motion workload | View | Owning group | Time change |
|---|---:|---:|---:|
| 20,000 linear movers | 78.47 us | 72.06 us | -8.2% |
| 20,000 rotating movers | 314.60 us | 257.53 us | -18.1% |
| 200,000 linear movers | 827.16 us | 763.88 us | -7.7% |
| 200,000 rotating movers | 3200.18 us | 2553.44 us | -20.2% |

The group is not enabled by default: it owns/reorders Transform and Velocity,
restricting independent sorting and conflicting owning groups. Setup cost,
transform propagation, rendering and entity churn during timing are excluded.
Positions and orientations match exactly by entity after all timed updates.
80/80 addon checks passed, including removal/expiry and repeated enable calls.
The temporary prebuilt-core/FreeImage workaround in direct-transforms.md applies.
