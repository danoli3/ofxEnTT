# Direct transforms

Apple M1 Max, 10 physical cores, 64 GiB RAM; macOS 26.6.2; Apple Clang 21.0.0;
ARM64 Release (`-O3 -DNDEBUG`). Medians from the benchmark described in ../README.md.

| Scene | Before | After | Elapsed time change |
|---|---:|---:|---:|
| 20,000 roots | 915.55 us | 396.89 us | -56.7% |
| 20,000 entities, chains of 8 | 1039.75 us | 773.06 us | -25.6% |

These are CPU transform updates, including checksums, not whole-frame or GPU times.
75/75 addon checks passed, including signed/zero scale equivalence; the benchmark
also checks every world matrix against the frozen pre-change implementation.

The local harness uses the installed prebuilt openFrameworks core. That archive
already bundles FreeImage, so the harness omits the duplicate standalone FreeImage
link. This workaround is confined to /tmp and is not part of the addon or its build.
