# OperandReader comparison

`OperandReader` now owns the per-instruction context and the small two-position
cache. Spawn and update handlers call `ReadValue`, `ReadScalar`, `ReadVector3`,
`ReadUInt`, and `ReadCurve` using named argument positions. This removes repeated
context parameters and explicit cache setup from the handlers. Integer and curve
reads keep their dedicated decoding paths. Source attributes are captured before
destination writes, and the cache does not limit operation argument count.

The reader measured **0.4482 ms** of mean GPU compute time against **0.4662 ms**
for the fresh array control, a **3.8% decrease in this collection**. This is not
evidence of a speedup caused by the abstraction. Both shaders have matching global
declarations and function instructions after normalizing SPIR-V IDs and global
declaration order. Their byte sizes, instruction counts, loads, and conditional
branch counts are unchanged. Raw binaries have different hashes because their
IDs and declaration order differ. No native GPU instruction or register comparison
was obtained.

## Fresh control results

Values pool all complete six-stage command buffers from two captures per variant.
They measure active GPU compute work, not total frame time.

| Stage | Array control | OperandReader | Observed change |
| --- | ---: | ---: | ---: |
| Scheduling | 0.0568 ms | 0.0590 ms | +4.0% |
| Spawn | 0.2202 ms | 0.1923 ms | -12.6% |
| Update | 0.1892 ms | 0.1969 ms | +4.0% |
| Total compute | 0.4662 ms | 0.4482 ms | -3.8% |

| Total compute statistic | Array control | OperandReader |
| --- | ---: | ---: |
| Complete GPU frames | 1,853 | 1,850 |
| Median | 0.4468 ms | 0.4361 ms |
| p95 | 0.5569 ms | 0.5638 ms |

The lower mean did not coincide with a lower p95. Spawn varied substantially
between the two reader captures, and update was slower in the pooled reader sample.
Workload phase, random inputs, system conditions, and native compiler behavior were
not isolated. The source refactor preserves the normalized SPIR-V instruction
structure; these measurements do not establish a runtime improvement or regression
caused by the reader.

| Capture | Complete GPU frames | Mean compute | Median | p95 |
| --- | ---: | ---: | ---: | ---: |
| operand-control-1 | 925 | 0.4656 ms | 0.4493 ms | 0.5605 ms |
| operand-reader-1 | 924 | 0.4362 ms | 0.4265 ms | 0.5661 ms |
| operand-reader-2 | 926 | 0.4603 ms | 0.4395 ms | 0.5615 ms |
| operand-control-2 | 928 | 0.4668 ms | 0.4443 ms | 0.5528 ms |

## Historical comparison

The previous small-array measurements are preserved in `SmallArrayComparison.md`.
They used the same baseline shader binaries, verified by SHA-256. Their mean total
was **0.4609 ms**, compared with **0.4482 ms** for this reader collection, an
observed **2.7% decrease**. This historical comparison spans separate sessions.

| Stage | Historical small array | OperandReader |
| --- | ---: | ---: |
| Scheduling | 0.0637 ms | 0.0590 ms |
| Spawn | 0.2203 ms | 0.1923 ms |
| Update | 0.1769 ms | 0.1969 ms |
| Total compute | 0.4609 ms | 0.4482 ms |

## Method and validation

Collected on 2026-10-03 using Apple M5, macOS, Release, and Vulkan/MoltenVK.
All four successful executions logged a 2560 x 1440 framebuffer. The default scene
remained FireAndFireworks plus RibbonVortex. FIFO presentation stayed enabled.
Each process warmed up for 45 seconds before a 15-second Metal System Trace
recording with Time Profiler. The parser filters events to Dissolve and sums the
six active depth-zero compute encoders in each complete command buffer: four
scheduling stages, spawn, and update.

Capture order was array control, reader, reader, array control. Each capture used
a separate app execution. No shader build or Vulkan tests overlapped captures;
all XML exports and analysis ran after the final capture. Both runtimes used the
same executable, shared library, assets, and other precompiled shaders. Runtime
hashes are in `operand-reader-runtime.json`. The original shaders were preserved
before editing; their sources match the saved `small-array-*.hlsl` snapshots.

One reader launch failed during startup before shaders loaded. Its crash report
points to `spdlog::logger::log_` called from `Elixir::Thread::WorkerLoop`. That
attempt produced no timing capture and was excluded. The launch was repeated
successfully without changing the executable, shared library, or shaders. Its
console log remains at `/tmp/aether-operand-reader-startup-failure.log` and the
system report is `Dissolve-2026-10-03-023021.ips` in DiagnosticReports.

Both shaders compiled through the project's DXC Shader Model 6.0 SPIR-V build and
passed `spirv-val --target-env vulkan1.3`. All **367 tests passed**, including
**31 GPU-labeled tests**. The normalized comparison retains named global identities
and compares complete function instruction sequences after remapping IDs; see
`compare-operand-reader.py`. It is a static comparison, not native GPU assembly.

| Shader | Bytes, both versions | Instructions, both versions | Loads | Conditional branches |
| --- | ---: | ---: | ---: | ---: |
| Spawn | 96,964 | 5,394 | 113 | 281 |
| Update | 84,584 | 4,676 | 72 | 327 |

This change adds only shader code and documentation. It changes no C++ API, shared
library symbol, CPU/GPU buffer layout, resource binding, or thread-group size.
The Windows build uses the same DXC SPIR-V compilation path, and no new
`ELIXIR_API` export is required. Windows execution and performance were not measured.
The captures cover one scene and GPU, with no deterministic replay or fixed GPU
frequency. Existing tests do not exhaustively read back every particle operation.

## Artifacts and reproduction

Per-capture metrics and CSVs, frozen reader shader sources, runtime hashes,
`operand-reader-tests.log`, and `operand-reader-comparison.json` are stored beside
this report. Raw traces and XML exports remain under `/tmp/aether-operand-*.trace`
and `/tmp/aether-operand-*.xml`. The profiling apps were stopped.

`profile-operand-reader.py` takes separate preserved control and reader runtime
directories. Each directory needs the executable, Assets, and its own Shaders.
For a new collection, move existing same-named traces out of `/tmp` first; the
script refuses to overwrite captures. Its `--captures` option resumes unfinished
captures after an interrupted run.

```sh
python3 Documentation/Profiling/AetherOperands/profile-operand-reader.py \
  --control /tmp/aether-operand-reader-control \
  --reader /tmp/aether-operand-reader-current

# Repeat this export for each of the four capture names.
python3 Documentation/Profiling/AetherOperands/export-aether-trace.py operand-control-1

python3 Documentation/Profiling/AetherOperands/aggregate-aether-profile.py \
  operand-control-1 operand-reader-1 operand-reader-2 operand-control-2

# Copy the resulting /tmp/aether-<capture>-{metrics.json,compute.csv} files into
# this report's directory as <capture>-{metrics.json,compute.csv}, then compare.
python3 Documentation/Profiling/AetherOperands/compare-operand-reader.py \
  --control-shaders /tmp/aether-operand-reader-control/Shaders/Aether \
  --reader-shaders /tmp/aether-operand-reader-current/Shaders/Aether
```
