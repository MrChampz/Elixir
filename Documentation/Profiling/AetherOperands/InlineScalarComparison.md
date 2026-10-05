# Inline scalar operands versus the constant table

This collection found **no consistent performance gain from inline scalar operands**.
Keeping scalars in the constant table measured **0.4799 ms** of mean
active GPU compute time versus **0.4920 ms** for inline scalars.
Inline scalars were **2.5% slower on average in this sample**.
Two pairs favored the table; one favored inline scalars. This is an observed
comparison on one GPU and scene, not proof of a general regression.
The measurements do not justify retaining the inline mask for performance alone
in this scene. The production encoding was unchanged during collection; the
subsequent simplification now stores scalar literals in constants.

## Compared behavior

The inline runtime uses the implementation at collection time: scalar literals in the first
16 argument positions store their float bits directly in the operand word, with
the corresponding bit set in `OperandMetadata`. Scalar readers can return that
value without a dependent constants-buffer read.

The table runtime stores every scalar literal as a reference to the constant
table. It retains the same typed operands, deduplication, compaction,
`OperandReader`, two-value cache, layouts, bindings, and dispatch sizes. Only
`ModuleCompileContext::Emit` encoding and the two inline-scalar checks in
`Operands.hlsl` differ. The table version removes these checks from the
shader instead of leaving an always-false mask branch.

Both profiling runtimes use the same executable and common simulator/system
objects. The common profiling code fixes the simulation delta to `1.0f / 60.0f`,
pauses before simulation frame 2400, and writes the compiled operations and
operand payloads before recording. It is generated outside the production source
tree by `prepare-literal-comparison.py`.

## GPU results

The primary comparison takes the first **1,191 complete compute frames per capture**,
so each variant contributes **3,573 frames**. Frames are ordered by GPU start time.
Every included command buffer has exactly six active depth-zero compute encoders:
four scheduling stages, spawn, and update. This measures active compute work,
not total frame latency or FPS. Percentages below are inline cost relative to
the table; positive means inline is slower.

| Stage | Constant table | Inline scalars | Inline change |
| --- | ---: | ---: | ---: |
| Scheduling | 0.0553 ms | 0.0584 ms | +5.6% |
| Spawn | 0.2255 ms | 0.2241 ms | -0.6% |
| Update | 0.1991 ms | 0.2095 ms | +5.2% |
| Total compute | 0.4799 ms | 0.4920 ms | +2.5% |

| Total compute statistic | Constant table | Inline scalars |
| --- | ---: | ---: |
| Median | 0.4736 ms | 0.4733 ms |
| p95 | 0.5759 ms | 0.5895 ms |

Spawn improves by only 0.6% in the pooled inline sample, while update is 5.2%
slower. The second inline capture accounts for much of the pooled difference.
Scheduling also varies even though its shader binaries are unchanged, which
shows that session conditions influence these measurements. GPU frequency and
thermal state were not fixed, and three pairs are insufficient to establish a
small causal improvement or regression.

| Pair | Inline total change | Inline spawn change | Inline update change |
| --- | ---: | ---: | ---: |
| 1 | +1.22% | -1.75% | +3.50% |
| 2 | +7.58% | +2.03% | +14.16% |
| 3 | -1.23% | -2.14% | -1.95% |

| Capture, in execution order | All complete frames | Mean, common prefix | Median, common prefix | p95, common prefix |
| --- | ---: | ---: | ---: | ---: |
| literal-inline-1 | 1,199 | 0.4871 ms | 0.4712 ms | 0.5785 ms |
| literal-table-1 | 1,198 | 0.4813 ms | 0.4728 ms | 0.5780 ms |
| literal-table-2 | 1,196 | 0.4792 ms | 0.4742 ms | 0.5708 ms |
| literal-inline-2 | 1,191 | 0.5155 ms | 0.5149 ms | 0.6117 ms |
| literal-inline-3 | 1,196 | 0.4733 ms | 0.4641 ms | 0.5719 ms |
| literal-table-3 | 1,195 | 0.4792 ms | 0.4743 ms | 0.5772 ms |

Pooling all complete frames instead gives **0.4801 ms** for the table
and **0.4921 ms** for inline scalars, also an observed **2.5%**
inline increase. Presentation remained near 60 FPS with FIFO enabled; the small
compute difference did not produce an observable FPS gain.

## Payload and shader checks

All six captured compilations decode to identical operation types, targets,
argument values, and emitter ranges. The comparison resolves scalar/vector
constant references and inline float bits, and checks their raw words rather
than comparing approximate floats. This checks the compiled instruction payloads;
it does not perform exhaustive GPU particle-state readback.

The scene contains 216 operations, 399 operand words, and 183 inline scalar
arguments. Inline storage uses 93 constant words instead of 210. That saves
**117 words / 468 bytes** of compiled payload: **5,424 bytes** instead of
**5,892 bytes**, including operations and operands. This does not reduce the
simulator's reserved GPU buffer capacities.

Both spawn/update variants compiled with the project's DXC Shader Model 6.0
SPIR-V settings and passed `spirv-val --target-env vulkan1.3`.

| Variant | Shader | Bytes | SPIR-V instructions | OpLoad | Conditional branches |
| --- | --- | ---: | ---: | ---: | ---: |
| inline | Spawn | 96,972 | 5,395 | 113 | 281 |
| inline | Update | 84,592 | 4,677 | 72 | 327 |
| table | Spawn | 94,560 | 5,217 | 113 | 268 |
| table | Update | 83,916 | 4,630 | 72 | 323 |

These are static SPIR-V counts. They do not count executed reads and do not
represent native Metal instructions or register pressure. Removing the inline
paths produces smaller SPIR-V shaders, but that alone does not establish their
runtime performance.

The current Release build passed **367 tests**, including **31 GPU tests**,
before profiling. This test run used the production inline encoding. The table
variant was checked by shader validation and the differential payload checks.
No build or GPU test overlapped the captures; export and analysis began after
all six recordings completed.

## Environment and limitations

Collected on **2026-10-04**, Apple M5, macOS 26.5.1, Release, Vulkan/MoltenVK,
2560 × 1440 framebuffer. The default scene was FireAndFireworks plus RibbonVortex.
Spawn and update use 256 threads per group in both variants. Historical captures
with different workload timing or group sizes are not used as the control.

Each recording runs for 20 seconds after Instruments reports it is recording
and the gate is released. A separate process warms to frame 2400 for each
capture. Order is inline/table, table/inline, inline/table. Equal frame prefixes
reduce differences in the duration of the sampled simulation phase. Fixed delta
and matching compiled inputs reduce workload variation, but the test does not
claim exhaustive deterministic GPU state equivalence.

The first attempt at `literal-table-3` failed during startup before shaders loaded
or a trace was recorded. The crash stack contains `spdlog::logger::log_` called
from `Elixir::Thread::WorkerLoop`. Its log is preserved as
`literal-table-3-startup-failure.log`; the system report is
`Dissolve-2026-10-04-010400.ips` in DiagnosticReports. The unchanged runtime was
restarted successfully. The failed launch contributes no timing samples.

The profiling experiment added scripts, snapshots, and results only. Production
C++ and HLSL source hashes matched their pre-comparison snapshots at collection
time. No type names,
public APIs, DLL symbols, bindings, or layouts were changed by this experiment;
no new `ELIXIR_API` export is needed. Windows compilation and performance were
not measured; the portable shader variants use the same DXC SPIR-V path.

## Artifacts and reproduction

`literal-comparison.json` contains pooled, per-capture, per-stage, and paired
results. Metrics, complete-frame CSVs, program manifests, logs, source snapshots,
`literal-build-manifest.json`, and `literal-baseline-tests.log` are beside this
report. Raw traces and XML exports remain in `/tmp/aether-literal-*.trace` and
`/tmp/aether-literal-*.xml`. The isolated runtimes remain in
`/tmp/aether-literal-comparison`; all profiling app processes were stopped.

The preparation script below requires the earlier implementation with inline
scalars and a matching Release build. It does not prepare variants from the
current constants-only implementation. The preserved isolated runtimes can still
be used with the capture script, skipping preparation.

```sh
# Use a Release build of the inline implementation with both targets built.
# Add the Vulkan SDK bin directory to PATH for spirv-val.
python3 Documentation/Profiling/AetherOperands/prepare-literal-comparison.py \
  --root "$PWD" --build /tmp/elixir-pr39-release \
  --output /tmp/aether-literal-comparison

python3 Documentation/Profiling/AetherOperands/profile-literal-comparison.py \
  --runtimes /tmp/aether-literal-comparison

# Run for each of literal-inline-1, literal-table-1, literal-table-2,
# literal-inline-2, literal-inline-3, literal-table-3.
python3 Documentation/Profiling/AetherOperands/export-aether-trace.py literal-inline-1

python3 Documentation/Profiling/AetherOperands/aggregate-aether-profile.py \
  literal-inline-1 literal-table-1 literal-table-2 \
  literal-inline-2 literal-inline-3 literal-table-3

# Copy /tmp/aether-<capture>-{compute.csv,metrics.json,programs.jsonl}
# beside this report as <capture>-{compute.csv,metrics.json,programs.jsonl}.
python3 Documentation/Profiling/AetherOperands/compare-literal-comparison.py \
  --runtimes /tmp/aether-literal-comparison
```

Use a fresh output directory and move existing traces aside for a new collection;
the scripts refuse to overwrite prepared runtimes or same-named traces.
Use `--captures literal-table-3` to resume a particular unfinished capture.
