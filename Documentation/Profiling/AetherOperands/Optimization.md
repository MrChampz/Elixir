# Aether operand optimization: repeated app profiling

The optimized interpreter reduces mean GPU compute time by **13.4%**
against the previously measured unoptimized operand implementation. Its total is
**+0.9%** relative to two fresh original-build controls.
The aggregate time is close to the original, but individual stages do not have full
parity: spawn is **+8.3%**, while update is
**-8.3%**. The remaining spawn regression must not
be hidden by the faster update stage.

A subsequent comparison after replacing cached reads with direct reads is in
`ReaderComparison.md`. The timing results in this report describe the cached version.

## Builds and changes

- Original build: `4e2ac26e12a3e114a3f052221ab99c522a995f77`, isolated under
  `/tmp/elixir-aether-before-build`.
- Unoptimized operand implementation: committed as `75fbe10` without profiling
  artifacts. Its earlier measurements are preserved in `Comparison.md`.
- Optimized build: the workspace changes after `75fbe10`, built in
  `/tmp/elixir-pr39-release`. These optimization changes remain uncommitted.
- No push was performed. Existing user changes and vendor modifications were preserved.

Scalar literals use their exact float bits in the operand word, removing a dependent
constant-buffer read. The instruction header remains 16 bytes: a 16-bit count and
16-bit scalar mask share one metadata word. Up to 65,535 arguments are supported;
positions beyond the first 16 use the reference path. This packing uses integer
shifts, not C++ bit fields. Finalization removes unused constants and rebases the
remaining shared vector/integer references. The None input folds to the literal one.

The first two arguments are decoded once per instruction, using named contract
indices. Additional arguments use typed readers. Spawn threads enumerate emission
order before mapping to the ring slot, keeping new particles together without
changing indices, order, or seeds. Spawn keeps 256 threads per group; update uses 64.
The CPU and HLSL group counts come from `ParticleDispatch.def`.

The final program reservation remains **3 MiB**, compared with **4 MiB** in the original.
A scalar instruction uses 20 bytes, a literal cone 44 bytes, and a nine-argument
literal vortex path 64 bytes. These are program-size calculations, not process-RSS
measurements. The instance record remains 60 bytes versus the original 52 bytes.
A subsequent direct Metal measurement found **no total graphics allocation reduction**:
both builds allocated **785.25 MiB**. See `GraphicsMemory.md` for the method and raw samples.

## Collection and comparison

The hardware, Release configuration, dependencies, assets, default scene, and trace
method match the earlier report. The app log confirms a **2560 x 1440 framebuffer**,
from its 1280 x 720 logical window. This is a MacBook Air with Apple M5 and MoltenVK.
Each measured app ran alone and warmed up for more than 40 seconds before recording.
Final captures did not overlap compilation or Vulkan test execution.

Instruments used Metal System Trace plus Time Profiler with 15-second limits. The
analysis filters all events to Dissolve and sums active Compute intervals for complete
command buffers with the same six stages. Final controls contain
**1848 complete GPU frames**; the optimized captures contain
**1838**. VSync stays enabled, so FPS cannot establish simulation parity.

| GPU stage | Original, fresh controls | Optimized | Change |
| --- | ---: | ---: | ---: |
| Scheduling | 0.0573 ms | 0.0608 ms | +6.2% |
| Spawn | 0.2066 ms | 0.2238 ms | +8.3% |
| Update | 0.1964 ms | 0.1800 ms | -8.3% |
| Total compute | 0.4603 ms | 0.4646 ms | +0.9% |

| Total compute statistic | Original, fresh controls | Optimized |
| --- | ---: | ---: |
| Median | 0.4418 ms | 0.4411 ms |
| p95 | 0.5613 ms | 0.5591 ms |

The earlier original mean was **0.4421 ms**; its fresh mean is
**0.4603 ms**. The optimized mean of **0.4646 ms** is still
**+5.1%** above that earlier absolute reference.
The matched current controls support the approximately 1% aggregate difference;
they do not erase the persistent spawn difference. Effects use normal elapsed time
and random inputs, with no deterministic replay or forced GPU power state. Desktop
activity, workload phase, and tracing can affect small differences. The measurements
cover one scene and one GPU; Windows performance was not measured.

The unoptimized operand mean was **0.5365 ms** in the earlier captures.
The 13% optimization comparison uses those historical captures, whereas original
versus optimized uses fresh controls. These comparisons are intentionally identified
separately rather than claiming all versions were captured at the same time.

## Iterations

These exploratory measurements guided selection; individual single captures are not
formal benchmarks. Typed readers alone reduced binary size without recovering GPU
time. Hoisting readers further reduced expansion, but inline literals produced the
clearer runtime improvement. Smaller groups improved update; spawn retains 256 threads.
Opcode switches were tested and discarded because they did not improve the observed
spawn timing. The final selection uses the original opcode if/else chain.

| Capture | Complete GPU frames | Mean compute | Median | p95 |
| --- | ---: | ---: | ---: | ---: |
| typed-1 | 924 | 0.6023 ms | 0.5585 ms | 1.1690 ms |
| typed-attributes | 925 | 0.5614 ms | 0.5382 ms | 0.6633 ms |
| hoisted-1 | 920 | 0.5564 ms | 0.5306 ms | 0.6479 ms |
| hoisted-2 | 926 | 0.5505 ms | 0.5227 ms | 0.6906 ms |
| hoisted-if | 927 | 0.5320 ms | 0.5109 ms | 0.6309 ms |
| group64-1 | 921 | 0.5285 ms | 0.4933 ms | 0.6105 ms |
| inline64-1 | 919 | 0.4882 ms | 0.4773 ms | 0.5973 ms |
| inline-switch-1 | 921 | 0.5005 ms | 0.4797 ms | 0.5880 ms |
| aligned-1 | 926 | 0.4793 ms | 0.4557 ms | 0.5678 ms |
| packed16-1 | 920 | 0.4733 ms | 0.4487 ms | 0.5620 ms |
| packed16-2 | 918 | 0.4560 ms | 0.4327 ms | 0.5514 ms |
| baseline-control-1 | 922 | 0.4645 ms | 0.4446 ms | 0.5637 ms |
| baseline-control-2 | 926 | 0.4560 ms | 0.4384 ms | 0.5560 ms |

`aligned-2` overlapped an incremental build and is excluded. Initial rolling-window
Game Performance captures from the earlier investigation also remain excluded.
Only the two `packed16` captures and two `baseline-control` captures establish the
final current comparison.

## Validation and artifacts

- All **367 tests passed**, including 31 Vulkan tests.
- DXC compiled spawn/update; SPIR-V validation passed for Vulkan 1.3. Reflection
  confirms a 16-byte instruction stride and 256/64 local thread sizes.
- New tests cover inline bit patterns, signed zero, binding separation, constant
  compaction, sharing, integer preservation, and repeated finalization.
- `verify-spawn-mapping.py` validates 91,520 cursor/capacity/count combinations
  against the old mapping. This is an algorithm check, not GPU particle readback.
- Shared headers use explicit integers and inline getters. No new out-of-line DLL
  API was introduced; existing compilation APIs remain exported with `ELIXIR_API`.
  Windows compilation/linking implications were inspected, but no Windows host was available.
- Final SPIR-V sizes: spawn 96,836 bytes / 5,382 static instructions; update
  84,488 bytes / 4,667 static instructions. Original sizes were 69,888 / 3,660 and
  80,092 / 4,425. Static counts are not executed per-particle instruction counts.

Raw final captures remain local:

- `/tmp/aether-packed16-1.trace`
- `/tmp/aether-packed16-2.trace`
- `/tmp/aether-baseline-control-1.trace`
- `/tmp/aether-baseline-control-2.trace`

Per-frame CSVs and metrics JSONs are saved alongside this report. Raw traces and
profiling artifacts were not added to the architecture commit. Test output is in
`/tmp/aether-packed16-all-tests.log`. The launched profiling apps were stopped.
