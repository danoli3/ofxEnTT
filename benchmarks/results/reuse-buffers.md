# Reusable frame buffers

Apple M1 Max (10 cores, 64 GiB), macOS 26.6.2, Apple Clang 21.0.0, ARM64 Release
`-O3 -DNDEBUG`. Method and workloads: ../README.md.

| Full CPU collection + matrix/color packing | Before | After | Time change |
|---|---:|---:|---:|
| 20,000 entities, 1 mesh | 840.69 us | 740.24 us | -11.9% |
| 20,000 entities, 32 meshes | 978.97 us | 796.29 us | -18.7% |

The reference is the previous PR's allocating entity-ID collector and registry
lookups. The candidate uses the actual internal collector and retained batch/map
storage, plus component pointers limited to a draw pass. Packing and checksum
work are included on both sides. The checksum of all packed values matches.
Root/expiry scratch capacity reuse is included in this change but not isolated
in these timings. CPU-only; there is no GPU drawing or frame-rate claim.

78/78 addon checks passed, including clear/recreate and refreshed pointer checks.
The same local prebuilt-core/FreeImage harness workaround documented in
 direct-transforms.md applies.
