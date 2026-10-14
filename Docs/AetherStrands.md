# GPU strands in Aether

`EParticleRenderMode::Strand` creates one persistent, simulated tube per live particle.
The particle is the root. Spawn and update modules still control its position, lifetime,
size, scale, and color. A separate compute pass moves the remaining control points
with Verlet integration, curl forces, and iterative distance constraints.

Open `Assets/VFX/GPUStrands.json` in the VFX editor for a working example. Select the
Strand render mode with the existing mode buttons. Edit the `strand` object in the
asset to configure point count, segment length, stiffness, damping, width, solver
iterations, radial sides, and spline subdivisions. Settings are
validated before compilation. They are literal values in this first stage.

The sample emits 64 roots once, using `"spawnRate": 0` and
`"burst": {"count": 64, "interval": 0}`. A zero burst interval means one initial
burst per instance start or reset. `"particleState": {"killOnLifetimeExpired": false}`
keeps the roots alive indefinitely. This avoids both lifetime-based death and circular
buffer replacement during normal preview playback. Positive burst intervals retain
periodic emission; trigger-driven emitters still wait for their trigger events.

## Update module targets

Update modules accept `"target": "Particle"` (the default) or
`"target": "StrandPoints"`. Particle modules update the root before the solver.
StrandPoints modules run for each free point on every internal integration step.
The root stays pinned. The compiler preserves order within each target and stores
two separate instruction ranges in the same shared operation/operand/constant tables.

`ApplyGravity` and `ApplyCurlNoise` support both targets. Other built-in modules
currently support Particle only. A StrandPoints module requires a Strand emitter.
Unsupported targets and incompatible modules are rejected during asset loading.

```json
"updateModules": [
  { "type": "ApplyGravity", "target": "StrandPoints", "gravity": [0, -0.15, 0] },
  { "type": "ApplyCurlNoise", "target": "StrandPoints", "strength": "$CurlStrength",
    "frequency": 2.2, "scrollVelocity": [0.12, 0.09, 0.07], "seed": 0 }
]
```

Define `CurlStrength` in the emitter or system parameter block to bind it. Curl
strength, frequency, scroll velocity, and the unsigned seed can use runtime bindings.
Changing an instance parameter updates the force without recreating the point state.
Removing the curl module removes its force. The solver contains no built-in curl or
gravity force. It still owns damping, root-motion drag, and structural constraints.

Both targets use the same velocity-update instructions. For strand points, the VM
starts with zero velocity and accumulates the force's velocity increment. Verlet
then multiplies that increment by the internal step duration, applying acceleration
with the expected squared-time factor exactly once. Forces are evaluated in the
emitter's simulation space and transformed into world space before integration.

Earlier `strand.gravity` and `strand.sway` JSON fields are accepted and translated
into StrandPoints modules before the explicit update list. Remove those legacy keys
when adding the equivalent modules to avoid applying a force twice.

## Attach the root to an instance

Use `"simulationSpace": "Local"` to make the roots follow the instance transform.
The solver transforms each particle position into world space before pinning its
root. The body keeps its world-space history, so moving an instance drags the tube
rather than moving the entire tube rigidly.

```cpp
instance->SetWorldTransform(glm::translate(glm::mat4(1.0f), position));
```

World-space emitters use particle positions directly, as other Aether modes do.
Segment length and radius use the largest transform-axis scale. Nonuniform scaling
therefore preserves circular tube sections rather than stretching them into ellipses.
Gravity is transformed from the emitter simulation space.

## Particle death policy

The emitter stack and emitter inspector show a **Particle State** section with a
**Kill on lifetime** checkbox. Changes use the document history, support undo/redo,
and recompile the preview. Missing state sections display the default without
modifying the asset until the checkbox changes.

`particleState.killOnLifetimeExpired` defaults to true for existing effects. Set it
false to keep particles alive after their authored lifetime has elapsed. Lifetime
remains a duration: it is not a sentinel for infinity. Age continues to increase,
and `NormalizedAge` remains `clamp(Age / max(Lifetime, 0.001), 0, 1)`. OverLife
modules therefore reach their final value and stay there. An opacity or size curve
that ends at zero can still hide a live particle.

Explicit kill modules, instance removal, and preview restarts retain their behavior.
Turning off lifetime-based death does not prevent continuous emission from replacing
particles when the circular buffer fills; use a single burst for persistent roots.

## State and rendering

Each control point consumes 48 bytes: position, previous position, and a transported
normal. `SResourcePoolLimits::StrandPointCapacity` defaults to 262,144 points. Only
strand emitters reserve point ranges. Allocation failure rolls back every range.
Ranges retire with their instance after the GPU has finished using them.

A spawn-order marker and instance generation reset the chain on respawn. Reused
point ranges are cleared before they are assigned to another instance. Replay uses
the same simulation steps and initializes fresh point state after a preview restart.

The tube renderer interpolates control points with Catmull–Rom splines and uses
transported normals to keep neighboring rings aligned. The material graph receives
particle color, continuous tube UVs, world position, and tube normals. Use
`"material": {"blendMode": "Opaque"}` for solid tubes with depth writes. Existing
particle effect materials keep their default translucent mode.

Vertex count per particle is `(points - 1) * subdivisions * radialSegments * 6`.
The sample uses 32 points, four subdivisions, and eight radial sides: 5,952 vertices
per particle. Lower these values when increasing strand count.

## First-stage limits

There are no collisions or self-collisions.
The solver accepts 2–96 points and 1–16 constraint iterations. It runs internal steps
of at most 1/60 second and caps simulated time at 8/60 second per submitted step to
avoid unstable jumps after long frame stalls. Lower stiffness permits stretching.
The current solver processes each chain sequentially in one GPU invocation.

## Strand playground

The Editor's **Strands** tab runs one persistent instance from
`Assets/VFX/StrandPlayground.json`. It uses 80 strands with 20 points each and
recreates the reference spheres and rounded boxes from particlez. These objects
are visible reference geometry; they do not participate in collision detection.

Use **W/A/S/D** to move the instance origin in the XY plane at 0.9 units per
second, within X = [-1.25, 1.25] and Y = [-0.95, 0.95]. Diagonal movement uses
the same speed. Drag with the left mouse button to orbit, use the middle button
to pan, and scroll to zoom. **Reset position** restores the origin and camera
without restarting the simulation. Movement keys are released when the tab changes
or another editor popup captures input. Simulation pauses while the tab is inactive.

Run `Editor --strands` to open this tab at startup on macOS or Windows. Its color,
depth, material, post-process, camera, and Aether resources belong to this scene
and remain separate from VFX asset previews.

`Assets/Meshes/StrandScene/generate.py` regenerates the reference glTF geometry
using only Python's standard library.

## Reusable strand samples

The strand emitter is a chain solver and tube renderer, independent of tentacle
behavior. Spawn modules place each root, particle updates control root attributes,
and updates targeting `StrandPoints` apply forces to the chain. The current solver
pins one end to its particle and leaves the other end free. It has no endpoint
attachment, bending/twist constraints, collision, or self-collision. The initial
chain shape is a procedural curl; these examples are visual approximations rather
than specialized cable or vegetation simulations.

Select **Tentacles**, **Cables**, **Seaweed**, or **Energy** in the Strands panel.
Switching samples replaces the instance but preserves its WASD-controlled position
and camera. Reference objects appear only with Tentacles. Each effect can also be
opened independently from the Content Browser in the VFX editor.

| Asset | Example | Configuration |
| --- | --- | --- |
| `StrandCables.json` | 12 hanging cables | Roots distributed along a horizontal line, strong downward gravity, tight distance constraints, weak noise. |
| `StrandSeaweed.json` | 48 seaweed stems | Roots spread across a bed, upward force approximates buoyancy, moderate stiffness and slow curl simulate a current. |
| `StrandEnergy.json` | 24 energy filaments | Thin blue tubes, strong curl, higher spatial frequency and faster noise scrolling. |

All samples use a single initial burst and disable death by lifetime. They retain
the same roots indefinitely until explicitly removed or restarted. Forces and
width differ between samples; they all use the same emitter and shader paths.
Energy uses the existing opaque particle material, without an emissive or glow
material. Cables have one anchored end and cannot connect two scene objects yet.
