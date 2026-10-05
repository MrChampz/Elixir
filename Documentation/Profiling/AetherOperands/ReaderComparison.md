# Operand reader comparison

Removing the two-argument cache was measured against both the previous cached captures
and a fresh cache-only control. All figures below are active GPU compute time, not frame time.

| Stage | Previous cache | Fresh cache control | Direct ReadOperand | Change vs fresh cache |
| --- | ---: | ---: | ---: | ---: |
| Scheduling | 0.0608 ms | 0.0613 ms | 0.0600 ms | -2.1% |
| Spawn | 0.2238 ms | 0.2247 ms | 0.2541 ms | +13.1% |
| Update | 0.1800 ms | 0.1780 ms | 0.1982 ms | +11.4% |
| Total | 0.4646 ms | 0.4640 ms | 0.5123 ms | +10.4% |

The subsequent experiment resolving all arguments before execution is documented in
`ResolveAllComparison.md`. The production-state descriptions below refer to this
earlier direct-reader experiment.

## Method

Apple M5, macOS, Release, MoltenVK; same default scene and 2560 x 1440 framebuffer.
VSync remained enabled. Each app ran alone, warmed up for at least 40 seconds, then
recorded for 15 seconds using Metal System Trace plus Time Profiler. The parser
sums six depth-zero active Compute encoders per complete command buffer: four
scheduling stages, spawn, and update. No compilation or GPU tests overlapped timing captures.
XML exports could overlap captures, adding CPU activity but no separate rendering workload.

The fresh cache control uses the current executable and shader sources, restoring only
the two early decoded arguments and ReadCachedOperand calls in temporary shaders.
All named indices, types, packing, group sizes, assets, and other shader logic stay the same.
Its SPIR-V validated for Vulkan 1.3. The production workspace continues to use ReadOperand.

Capture order: direct-1, cache-control-1, cache-control-2, direct-2. A separate
counter experiment followed direct-1 and is excluded from timing statistics.
The controls were measured in one app execution, and the direct captures in two
separate executions. Effects retain elapsed time and random inputs; there is no
deterministic replay or locked GPU frequency. Two captures per variant describe
this scene and machine, not a cross-device performance guarantee.

| Capture | Complete frames | Mean total compute |
| --- | ---: | ---: |
| packed16-1 | 920 | 0.4733 ms |
| packed16-2 | 918 | 0.4560 ms |
| cache-control-1 | 920 | 0.4781 ms |
| cache-control-2 | 927 | 0.4501 ms |
| direct-1 | 926 | 0.5146 ms |
| direct-2 | 928 | 0.5100 ms |

## Shader expansion and registers

| Shader | Cached bytes | Direct bytes | Cached static instructions | Direct static instructions |
| --- | ---: | ---: | ---: | ---: |
| Spawn | 96,964 | 147,300 | 5,394 | 8,778 |
| Update | 84,584 | 144,608 | 4,676 | 8,654 |

These are static SPIR-V counts, not native Apple GPU instructions, executed work,
or register counts. Direct reads expand more generic decoding into opcode branches.

A separate Metal GPU Counters recording returned:
`Selected counter profile is not supported on target device`. Its counter-info
export contained no rows. The old shader-list table exposes labels and PC ranges,
but no register allocation counts. The local Xcode installation also lacks the
Metal Toolchain (`xcrun metal` reports it missing). No valid register count, register
pressure, spilling, or occupancy comparison was obtained. Shader size alone cannot
establish any of those metrics.

## Artifacts

Per-capture CSVs, JSON metrics, control shader sources, the control preparation script,
and `reader-comparison.json` are saved alongside this report. Raw traces remain under
`/tmp/aether-{direct-1,direct-2,cache-control-1,cache-control-2}.trace`; the excluded
counter attempt is `/tmp/aether-direct-counters.trace`. Profiling artifacts remain
uncommitted. The profiling apps were stopped.
