# Aether operand architecture: before/after profiling

The new operand architecture reduced the reserved program buffers by 1 MiB but
increased GPU simulation time in this scene. The measured GPU regression was
reproduced in two captures per version. FPS stayed at approximately 60 because
both versions use FIFO presentation with VSync.

## Setup

- Date: 2026-10-02.
- Hardware: MacBook Air with Apple M5 CPU/GPU, macOS 26.5.1.
- Build: Release, Apple Clang, C++20, `ELIXIR_PROFILE=OFF`; external Instruments
  profiling was enabled equally for both binaries.
- Vulkan/MoltenVK: Vulkan SDK 1.4.357.1, same dependencies and assets for both builds.
- Before: commit `4e2ac26e12a3e114a3f052221ab99c522a995f77`, extracted to
  `/tmp/elixir-aether-before`. The existing user change to ModuleParseContext.h was
  copied into this checkout so that it matched the current workspace.
- After: current uncommitted implementation in `/Users/felpsmac/projects/Elixir`.
- App: Dissolve, default camera and scene, 1280 x 720 initial logical window size.
- Effects: FireAndFireworks and RibbonVortex, one instance of each. Their total
  configured particle capacity is 14,556; this is capacity, not a measured live count.
- Each version ran alone. Startup was excluded; capture began after more than
  40 seconds of process uptime. Two continuous captures per version used the
  Metal System Trace template with the Time Profiler instrument, 15-second limits.
- Collection retained approximately 15.4 seconds per run. Analysis filters GPU,
  presentation, and CPU events to the Dissolve process. Other app events are excluded.

The initial Game Performance captures retained less than one second of data and
were excluded from the comparison. The final captures use continuous Metal tracing.

## GPU results

Times below sum active Compute GPU intervals per frame command buffer. Only complete
buffers containing six compute encoders are included. The code submits six Aether
compute stages in this order: scheduler begin, initialize emitters, schedule emitters,
finalize events, spawn, and update. A source scan found no other per-frame compute
dispatches in the app. Attribution to these stages follows that command order; the
Release Metal encoder labels do not contain individual shader names.

| Metric | Before | After | Change |
| --- | ---: | ---: | ---: |
| Scheduling total | 0.0572 ms | 0.0572 ms | -0.0% |
| Spawn | 0.2064 ms | 0.2624 ms | +27.1% |
| Update | 0.1784 ms | 0.2170 ms | +21.6% |
| Simulation total | 0.4421 ms | 0.5365 ms | +21.4% |
| Simulation total, p95 | 0.5557 ms | 0.6385 ms | +14.9% |

Complete GPU frames analyzed: **1853 before and
1853 after**. Present requests: 1854 before
and 1853 after; edge frames can be partially outside the capture.

| Capture | Mean simulation time | Median | p95 |
| --- | ---: | ---: | ---: |
| before-continuous | 0.4481 ms | 0.4336 ms | 0.5486 ms |
| before-repeat | 0.4361 ms | 0.4282 ms | 0.5679 ms |
| after-continuous | 0.5379 ms | 0.5300 ms | 0.6408 ms |
| after-repeat | 0.5352 ms | 0.5191 ms | 0.6348 ms |

The increase is concentrated in spawn and update, the stages that interpret the
new operand references. Scheduling stayed close to its previous cost. Vertex and
fragment work stayed in a similar range and was not the source of this regression.
GPU channel durations must not be added together to claim total frame latency:
channels can overlap. The compute sum measures simulation work, not presentation latency.

## CPU and presentation

Simulator inclusive CPU time estimated by 1 ms sampling was **0.0394 ms/frame before** and **0.0367 ms/frame after**. The simulator accumulated only 73 and 68 samples respectively, so this does not establish a CPU speedup.

Whole-process sampled CPU time averaged **7.22 ms/frame before** and **7.59 ms/frame after**. This includes GUI, rendering, the Vulkan/Metal driver, and tracing effects. The before runs varied from 6.96 to 7.48 ms/frame, so a small global CPU difference needs more controlled repeats to attribute it to this change.

All four runs averaged approximately **16.668 ms between present requests**, or
60 Hz. VSync masks the additional simulation cost; unchanged FPS is not evidence
that the new shaders have the same cost.

## Memory and shader complexity

The configured program-buffer reservation is **4 MiB before** (instructions only)
and **3 MiB after** (instructions, operands, constants), a calculated 25% reduction.
This is a buffer-size calculation, not a measured 25% reduction in total app memory.
The instance record grows from 52 to 60 bytes. This scene has one instance per effect,
so it does not exercise the benefit of sharing a program between repeated instances.
Resident app memory varied with MoltenVK/compiler caches and Instruments attachment;
RSS snapshots were not treated as a reliable architecture comparison.

The compiled SPIR-V grew substantially:

| Shader | Before bytes | After bytes | Before static instructions | After static instructions |
| --- | ---: | ---: | ---: | ---: |
| Spawn | 69,888 | 167,524 | 3,660 | 10,232 |
| Update | 80,092 | 144,456 | 4,425 | 8,665 |

Static conditional branches increased from 185 to 941 in spawn and from 316 to 787
in update. These are binary instruction counts, not executed per-particle counts.
They are consistent with the generic operand reader being expanded at many call sites.
Extra reference loads and source-selection code are a plausible explanation for the
measured regression; register pressure and individual instruction costs were not
isolated by this capture.

## Conclusion and limits

In this Mac/MoltenVK scene, the current implementation trades lower reserved program
memory and a more extensible representation for roughly 21% more GPU simulation work.
It is not a performance improvement yet. The next optimization should specialize
operand reads by required type and avoid expanding the full generic source/attribute
selection for every argument, followed by the same before/after measurements.

These results cover one default scene and one GPU. Effects use their normal time and
random inputs, rather than a deterministic replay. GPU power state was not forced.
Small differences require caution; the spawn/update increase persisted in both runs.
Windows and stress tests with repeated instances were not measured. No production
source was changed for profiling, and nothing was committed or pushed.

## Raw captures and reproduction

Final captures are retained locally:

- `/tmp/aether-before-continuous.trace`
- `/tmp/aether-before-repeat.trace`
- `/tmp/aether-after-continuous.trace`
- `/tmp/aether-after-repeat.trace`

Example collection command:

```sh
xctrace record --template 'Metal System Trace' --instrument 'Time Profiler' \
  --attach <Dissolve-PID> --time-limit 15s --output /tmp/aether-before-continuous.trace
```

The export script reads these captures and writes XML tables into `/tmp`. The
aggregation script uses the adjacent parse helper and writes metrics plus one CSV
row per complete compute frame. Saved CSV and JSON files in this directory contain
only the analyzed app metrics, not the trace environment or other processes.
