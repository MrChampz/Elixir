# Actual Metal resource allocation comparison

No reduction in total Metal resource allocation was measured for the default scene.

| Build | Samples | Min / max allocated bytes | MiB |
| --- | ---: | ---: | ---: |
| Original (4e2ac26) | 15 | 823,394,304 / 823,394,304 | 785.25 |
| Optimized workspace | 15 | 823,394,304 / 823,394,304 | 785.25 |
| Difference | | 0 | 0 |

Hardware: Apple M5, unified memory. Both Release builds used the same default scene
and 2560 x 1440 framebuffer. Each app ran alone. A profiling-only injected dylib
sampled MTLDevice.currentAllocatedSize once per second for 15 seconds, after a
25-second startup delay. The native Metal SDK defines this property as the size in
bytes of all resources allocated by the device. This measures Metal resource
allocation in the app, not process RSS, physical residency, or system-wide GPU use.

The logical program buffers shrink from 4 MiB to 3 MiB, with 2 KiB added to the
instance buffer. That saving did not translate into a smaller total Metal resource
allocation in this test. Larger allocator blocks can absorb changes to suballocated
buffer sizes; this is a plausible explanation, not a measured allocation breakdown.
Results cover one run of each build on this scene and GPU; dedicated VRAM on Windows
was not measured. The apps were stopped after sampling.

The previous Metal System Trace captures contained no rows in their
metal-current-allocated-size tables, so those captures cannot support memory claims.
The probe source and raw JSONL samples are stored alongside this report. These are
profiling artifacts, excluded from the implementation commit.
