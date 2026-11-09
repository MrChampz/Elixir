# Application-owned vegetation on Aether

Aether owns particle storage, scheduling, integration, generic distance constraints, and
immutable render frames. It has no growth states, branch counts, plant maturity, climbing
rules, or vegetation playback controls in its API or built-in shaders.

The Editor owns those policies in `Editor/Source/VFX/Vegetation`, their compute shader in
`Editor/Shaders/VegetationConstraints.cs.hlsl`, and tube/leaf/balloon rendering in its own
shader package. `EStrandGrowthState` is an Editor type. The six vegetation examples remain
available in the Strands tab and in VFX documents.

## Generic Engine extension

Any emitter can set an immutable `Simulation::ParticleProcessor` through
`Emitter::SetParticleProcessor()`. The extension is independent of Sprite, Mesh, Ribbon,
Strand, or None rendering. Compilation asks the application whether the emitter satisfies
its processing contract. A compiled snapshot retains the description even if authoring
later assigns a different processor.

Each Simulator creates a separate `ParticleProcessorRuntime`. After particle integration
and built-in constraints, it calls `Record()` with the emitter's allocation, particle
layout, simulation step, world transform, and recording command buffer. The application
can record compute or transfer work and must stay within that allocation. It must not end
or submit the command buffer. Aether supplies barriers before and after application work
and publishes the final particles for rendering.

The runtime owns its shaders, pipelines, descriptors, and uploads. It must retain resources
until their graphics frame slot is safe to reuse and must not update descriptors already
used in recorded commands. Simulator caches runtimes per description and retires unused
runtimes after the graphics frame slots are safe. Resource creation failure is logged and
stops the affected application stage safely.

`EParticleRenderMode::None` keeps simulation and immutable frame publication active without
submitting built-in geometry. Applications can render those particles using the existing
`Manager::OnRenderFrame()` hook and their own `SEmitterRenderConfig` subclass. This is the
path used by Editor vegetation; no Engine renderer interprets vegetation metadata.

The built-in CoreV1 representation remains 96 bytes. The two unused words in `Transform.zw`
are preserved by ordinary particle updates and reset on spawn. Their interpretation belongs
to the application. Built-in simulation and renderers do not assign them growth semantics.

## Editor authoring and playback

The editable JSON retains `StrandGrowth`, optional endpoint/surface settings on the distance
module, tube taper, and optional leaf/balloon renderers. `ParseVegetationEffect()` extracts
these application fields, validates them, and delegates only generic modules to the Engine
parser. It then attaches the sample processor and its immutable rendering configuration.
The Engine parser does not register or recognize `StrandGrowth`.

Ordinary Aether effects pass through unchanged and have no application processor. Effects
using the Editor vegetation schema must be loaded through the Editor parser, rather than
through `Aether::Effect::ParseEffect()` directly.

A sample contains one growth module and one distance module. Growth can be Growing, Mature,
or Paused. Zero speed starts fully developed. The runtime pause button stops the Editor's
development clock while ordinary particle physics continues. Branch location and delay are
seeded; a branch starts after its parent reaches the selected attachment point.

Budgets use interleaved lanes:

- `orderStep = mainChainCount * (1 + branchCount)`.
- `maxParticles = maxPoints * orderStep`.
- Tube and simulation topology must match.
- Spawn the full capacity in one initial burst, with `spawnRate = 0`, burst interval zero,
  and `particleState.killOnLifetimeExpired = false`.
- Use `SetGroupIdFromSpawnOrder` with `groupCount = orderStep` and `firstGroupId = 0`.

The Editor solver owns each main chain and its branches in one invocation, so branch
anchors read completed parent positions. It supports simulation/world endpoints and static
analytic planes, spheres, and rounded boxes. These are application guidance surfaces,
not Engine mesh collision. Fixed endpoints require mature or zero-speed growth. An
unreachable endpoint cannot satisfy every segment distance simultaneously.

Open the Strands tab to select Ivy, Climbing Ivy, Fern, Willow, Power Lines, or Balloons.
Drag to orbit, use the wheel to zoom, and use WASD to move the instance. Power Lines stays
fixed with its reference poles. The original four generic strand examples remain available.
Leaves and balloons consume the immutable application configuration and particle buffer;
all custom geometry renders into the preview's existing color and depth targets before
postprocessing.

## Validation

Engine tests verify processor compatibility and immutable snapshots for every emitter render
mode. A Vulkan test verifies actual particle writes for all five modes and separate runtime
resources when a description is shared between Simulators. Existing distance-constraint and
strand-rendering tests continue to exercise only generic Engine behavior.

`Editor.VFXTests` covers sample parsing, budgets, settings, and the application/Engine
boundary. Its GPU test simulates and renders all six examples, verifies fixed endpoints,
and checks that pausing preserves development while the simulation continues. GPU tests
are labeled `gpu`; the Editor shader package is a build dependency of that test target.
New Engine constructors and destructors have out-of-line `ELIXIR_API` definitions for
Windows DLL consumers. Native Windows compilation and linking require Windows CI.

Validated on macOS ARM64 after this separation: Engine, Editor, Dissolve, and their affected
test targets compiled and linked. All 952 tests without GPU dependencies passed. Eight
Engine Vulkan tests passed, including the generic processor test across all render modes;
the Editor GPU integration test passed for all six samples and their playback controls.
