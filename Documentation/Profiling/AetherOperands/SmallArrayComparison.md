# Small operand cache as a float4 array

The subsequent OperandReader refactor and fresh control measurements are documented
in `OperandReaderComparison.md`. This report preserves the earlier array experiment.

The shader now caches only the first two arguments in float4 arguments[2].
ReadCachedOperand receives this array, returns cached values for indices 0 and 1,
and resolves higher indices through ReadOperand. Dedicated scalar/vector/UInt/curve
readers remain for the appropriate arguments. The full-array resolver and its nine-argument
CPU/HLSL capacity limit were removed. The cache does not limit operation argument count.

The compiled spawn and update SPIR-V binaries are byte-for-byte identical to the
previous cache with separate argument0 and argument1 values (verified by SHA-256).
Therefore differences between those source variants in timing samples must not be
interpreted as a code-induced speedup or regression.

| Stage | Previous separate-value cache | Previous full array | Small array cache |
| --- | ---: | ---: | ---: |
| Scheduling | 0.0619 ms | 0.0568 ms | 0.0637 ms |
| Spawn | 0.2212 ms | 0.2610 ms | 0.2203 ms |
| Update | 0.1706 ms | 0.2086 ms | 0.1769 ms |
| Total | 0.4537 ms | 0.5264 ms | 0.4609 ms |

Total compute is 12.4% lower than the prior full-array experiment.
The full-array comparison uses historical captures; the binary equivalence with
the separate-value cache is stronger evidence of preserving that implementation.

## Collection

Apple M5, macOS, MoltenVK, Release, same default scene and 2560 x 1440 framebuffer.
The app ran alone and warmed up for at least 40 seconds before two sequential
15-second captures in one execution. Metal System Trace plus Time Profiler match
the previous measurements. No compilation or GPU tests overlapped captures.
XML export of the first capture could overlap the second and add CPU work.

The parser sums six active depth-zero compute encoders per complete command buffer:
four scheduling stages, spawn, and update. This measures GPU compute time, not
full frame time; VSync stays enabled. Effects use elapsed time and random inputs
without deterministic replay or forced GPU frequency. Workload phase and system
conditions can cause sample variation, particularly in update. This scene and GPU
do not establish performance on Windows or other GPUs.

| Capture | Complete frames | Mean compute | Median | p95 |
| --- | ---: | ---: | ---: | ---: |
| small-array-1 | 919 | 0.4669 ms | 0.4485 ms | 0.5712 ms |
| small-array-2 | 928 | 0.4549 ms | 0.4261 ms | 0.5627 ms |

## Static shader comparison

| Shader | Bytes, both cache variants | Static instructions, both variants |
| --- | ---: | ---: |
| Spawn | 96,964 | 5,394 |
| Update | 84,584 | 4,676 |

Static SPIR-V counts are not native register counts. The counter-profile
incompatibility documented in ReaderComparison.md still applies; no valid register
or occupancy metric was obtained.

## Validation and artifacts

All 367 tests passed, including 31 Vulkan tests. DXC compilation and SPIR-V validation
for Vulkan 1.3 passed. No new out-of-line shared-library API was introduced; the
experimental C++ capacity constant and guard were removed. Windows DLL export/import
implications were checked, but no Windows execution was performed.

Metrics, per-frame CSVs, shader snapshots and SHA-256 hashes are saved alongside
this report. Raw traces are /tmp/aether-small-array-{1,2}.trace. The app was stopped.
The implementation and profiling artifacts remain uncommitted; no push was performed.
