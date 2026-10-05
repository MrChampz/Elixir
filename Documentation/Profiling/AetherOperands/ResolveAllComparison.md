# Resolve all operands before executing operations

Resolving all arguments into a float4 array was measured against previous cached captures
and a fresh cache-only control. All figures below are active GPU compute time, not frame time.

| Stage | Previous cache | Fresh cache control | All arguments resolved | Change vs fresh cache |
| --- | ---: | ---: | ---: | ---: |
| Scheduling | 0.0613 ms | 0.0619 ms | 0.0568 ms | -8.2% |
| Spawn | 0.2247 ms | 0.2212 ms | 0.2610 ms | +18.0% |
| Update | 0.1780 ms | 0.1706 ms | 0.2086 ms | +22.3% |
| Total | 0.4640 ms | 0.4537 ms | 0.5264 ms | +16.0% |

This implementation is not recommended for performance: total compute rose by 16.0%
against the fresh two-argument cache control, or about 0.0727 ms per frame. It is also
about 2.8% slower than the previously measured direct readers (0.5123 ms), although
that latter comparison uses historical captures. The current results favor retaining
a small cache over resolving every argument into a variable-index local array.
The implementation remains in the workspace for review; it was not reverted.

This experiment was subsequently replaced by the small float4 arguments[2] cache.
See SmallArrayComparison.md for the current implementation and measurements.

## Method

Apple M5, macOS, Release, MoltenVK, same default scene and 2560 x 1440 framebuffer.
VSync stayed enabled. Each app ran alone, warmed up for at least 40 seconds, then
recorded for 15 seconds using Metal System Trace plus Time Profiler. The parser sums
six depth-zero active compute encoders per complete command buffer: four scheduling
stages, spawn, and update. No compilation or GPU tests overlapped timing captures.
XML exports could overlap captures and add CPU work.

The cached control uses preserved pre-change executables and shaders from the
previous cache comparison. Shader SHA-256 hashes are saved in reference-hashes.json.
Its engine library is shared with the current build; the new CPU capacity check
does not change emitted programs for the supported contracts. Packing, parameters,
assets, particle counts, operation contracts, and dispatch sizes remain the same.

The experimental shader calls ResolveOperands once per instruction. It loops over
the actual argument count, writes a local float4[9], then handlers read named positions.
Numeric arguments use ReadOperand. UInt and curve arguments preserve raw bits in x,
recovered with asuint/asint. ReadCachedOperand remains defined but unused. The explicit
capacity is mirrored in C++ and HLSL and validated by the C++ module compiler.

Capture order: resolve-all-1, array-cached-1, array-cached-2, resolve-all-2.
Cached controls share one execution; experimental captures use separate executions.
Effects use elapsed time and random inputs, with no deterministic replay or locked
GPU frequency. This is a two-capture comparison on one scene and GPU, not a general
performance guarantee.

| Capture | Complete frames | Mean compute | Median | p95 |
| --- | ---: | ---: | ---: | ---: |
| cache-control-1 | 920 | 0.4781 ms | 0.4498 ms | 0.5593 ms |
| cache-control-2 | 927 | 0.4501 ms | 0.4219 ms | 0.5643 ms |
| array-cached-1 | 920 | 0.4654 ms | 0.4461 ms | 0.5515 ms |
| array-cached-2 | 927 | 0.4421 ms | 0.4098 ms | 0.5765 ms |
| resolve-all-1 | 923 | 0.5284 ms | 0.5030 ms | 0.6172 ms |
| resolve-all-2 | 919 | 0.5244 ms | 0.5026 ms | 0.6272 ms |

The previously measured direct readers (without caching) averaged 0.5123 ms total compute.

## Static shader size

| Shader | Cached bytes | Resolved array bytes | Cached instructions | Resolved array instructions |
| --- | ---: | ---: | ---: | ---: |
| Spawn | 96,964 | 73,212 | 5,394 | 3,847 |
| Update | 84,584 | 79,116 | 4,676 | 4,310 |

Static SPIR-V size and instruction counts do not measure executed work or native GPU
register allocation. The SPIR-V contains a function-local nine-element float4 array
with variable-index stores. Local array accesses may carry a runtime cost; the trace
does not establish register spilling or identify that as the cause of the measured change.
No new valid register/occupancy metric was obtained; the counter-profile incompatibility
from ReaderComparison.md still applies.

## Validation and artifacts

Both shaders compiled with DXC and passed spirv-val for Vulkan 1.3. All 367 tests
passed, including 31 Vulkan tests. C++/HLSL capacities match; handlers contain no
operand-reader calls. The SPIR-V preserves UInt/curve payloads through OpBitcast.
Existing tests do not provide an exhaustive GPU readback of all operation contracts.
No out-of-line shared-library API was added, so no new Windows DLL export is needed.
Windows execution and performance were not measured.

Per-capture metrics, CSVs, frozen shader sources, and resolve-all-comparison.json
are stored alongside this report. Raw traces remain in /tmp/aether-*.trace.
The profiling apps were stopped. No commit or push was performed.
