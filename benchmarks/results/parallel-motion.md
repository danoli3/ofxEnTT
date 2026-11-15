# Optional parallel CPU updates

Apple M1 Max (10 physical cores, 64 GiB), macOS 26.6.2, Apple Clang 21, ARM64 Release
`-O3 -DNDEBUG`. One calling thread versus four total threads (three persistent
workers plus caller). Workloads, checksums and warmup: ../README.md.

| Workload | Serial | Four total threads | Time change |
|---|---:|---:|---:|
| Complete World update / 20k linear | 397.81 us | 437.34 us | +9.9% |
| Complete World update / 20k rotating | 610.64 us | 504.71 us | -17.3% |
| Complete World update / 200k linear | 4024.99 us | 4152.20 us | +3.2% |
| Complete World update / 200k rotating | 6290.26 us | 4675.93 us | -25.7% |
| SoA / 20k (small-input serial fallback) | 6.85 us | 6.79 us | -0.8% |
| SoA / 200k | 73.78 us | 87.52 us | +18.6% |
| SoA / 2M | 814.12 us | 646.09 us | -20.6% |

The full World comparison includes pointer gathering, scheduling/joins, expiry
scans, deferred-command draining and serial transform propagation. Both sides use
owning groups. SoA timings include scheduling/joins but no ECS scatter. Thread
creation and first allocations are excluded by warmup. The 20k SoA case stays on
the caller because it is below twice the default grain size; its tiny difference
is noise, not parallel scaling. Cheap motion can lose after parallel overhead;
all parallel paths remain opt-in, with default one-thread behavior.

## Correctness and sanitizer evidence

- 110/110 addon checks pass in Release and in a Debug ASan/UBSan build with
  `ASAN_OPTIONS=detect_container_overflow=0` (qualification below).
- The standalone `benchmarks/tests/parallel_stress.cpp` passes 700 uneven jobs,
  repeated pool resizing and worker exception recovery under ThreadSanitizer.
- The same stress check passes the native-compiled threadless fallback selected
  with `-D__EMSCRIPTEN__`; this is not a full WebAssembly runtime validation.
- Benchmark positions and orientations match serial results within 1e-4/1e-5.
- Tests cover synchronous joins before expiry/hierarchy updates, command draining,
  commands queued during draining, small/empty inputs, and nested-job rejection.

The installed openFrameworks core is prebuilt without ASan. Unrestricted container
annotation checks abort while creating its no-window renderer, before addon tests.
`benchmarks/tests/core_asan_repro.cpp` reproduces a container-overflow during core
renderer setup without linking any addon source. Mixing instrumented and
uninstrumented vector code is a documented source of false positives:
https://github.com/google/sanitizers/wiki/AddressSanitizerContainerOverflow#false-positives
A fully ASan-built core is needed for an unrestricted container-checking result.
Container annotation checking alone was disabled for the qualified full-suite run;
other ASan/UBSan checks remained enabled. No suppression is added to production.
The temporary harness also omits the duplicate standalone FreeImage link already
bundled in that local core, as in the preceding results. No GPU work is measured.
