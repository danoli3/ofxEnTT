# Optional incremental transforms

Apple M1 Max (10 cores, 64 GiB), macOS 26.6.2, Apple Clang 21, ARM64 Release
`-O3 -DNDEBUG`. 20k entities; method and edit pattern: ../README.md.

| Scene / local edits per frame | Full updates | Incremental | Time change |
|---|---:|---:|---:|
| Roots / 0% | 376.14 us | 386.67 us | +2.8% |
| Roots / 1% | 382.14 us | 392.01 us | +2.6% |
| Roots / 100% | 453.21 us | 568.03 us | +25.3% |
| Chains of 8 / 0% | 653.73 us | 538.78 us | -17.6% |
| Chains of 8 / 1% | 654.69 us | 541.88 us | -17.2% |
| Chains of 8 / 100% | 721.22 us | 864.89 us | +19.9% |

This is an opt-in tradeoff, not a universal speedup. Hierarchy visits, edit writes,
comparisons, cache pruning, and checksums are included. Cache setup and memory are
excluded. Direct public Transform writes are detected by snapshots; unchanged
matrices do not emit WorldTransform update notifications in incremental mode.
All resulting matrices match full updates within relative tolerance 1e-4.
91/91 addon checks passed. The temporary prebuilt-core/FreeImage workaround from
 direct-transforms.md applies. No GPU or frame-rate timing is included.
