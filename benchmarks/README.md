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
