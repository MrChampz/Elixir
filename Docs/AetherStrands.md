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

There are no general particle collisions or self-collisions. Growing strands
support static analytic surface guidance and attachment.
The solver accepts 2–96 points and 1–16 constraint iterations. It runs internal steps
of at most 1/60 second and caps simulated time at 8/60 second per submitted step to
avoid unstable jumps after long frame stalls. Lower stiffness permits stretching.
The current solver processes each chain sequentially in one GPU invocation.

## Strand playground

The Editor's **Strands** tab runs one persistent instance from
`Assets/VFX/StrandPlayground.json`. It uses 80 strands with 20 points each and
recreates the reference spheres and rounded boxes from particlez. These objects
are visible reference geometry in the Tentacles sample. Climbing Ivy also
uses matching analytic surfaces for guidance and attachment.

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
pins the main root to its particle and branch roots to parent points, leaving
the tips free. It has no second-tip attachment, bending/twist constraints,
mesh collision, or self-collision. Analytic surfaces can guide and pin growing
segments. The initial
chain shape is a procedural curl; these examples are visual approximations rather
than specialized cable or vegetation simulations.

Select **Tentacles**, **Cables**, **Seaweed**, **Energy**, or **Ivy** in the Strands panel.
Switching samples replaces the instance but preserves its WASD-controlled position
and camera. Reference objects appear only with Tentacles. Each effect can also be
opened independently from the Content Browser in the VFX editor.

| Asset | Example | Configuration |
| --- | --- | --- |
| `StrandCables.json` | 12 hanging cables | Roots distributed along a horizontal line, strong downward gravity, tight distance constraints, weak noise. |
| `StrandSeaweed.json` | 48 seaweed stems | Roots spread across a bed, upward force approximates buoyancy, moderate stiffness and slow curl simulate a current. |
| `StrandEnergy.json` | 24 energy filaments | Thin blue tubes, strong curl, higher spatial frequency and faster noise scrolling. |
| `StrandIvy.json` | 8 hanging ivy vines | Growing brown stems with green leaves, three side branches, downward gravity and mild wind. |

All samples use a single initial burst and disable death by lifetime. They retain
the same roots indefinitely until explicitly removed or restarted. Forces and
width differ between samples; they all use the same emitter and shader paths.
Energy uses the existing opaque particle material, without an emissive or glow
material. Cables have one anchored end and cannot connect two scene objects yet.

## Editor leaf renderer

`StrandIvy.json` uses an optional application renderer implemented entirely in
`Editor/Source/VFX/Rendering` and `Editor/Shaders`. Engine strands remain generic:
they simulate chains and render tubes. Engine code contains no leaf settings,
leaf silhouettes, leaf geometry, or plant shading.

The Editor manager composes `LeafRenderer` and overrides Aether's generic protected
`Manager::OnRenderFrame()` hook. Aether calls the hook after recording the final
simulation step, including timeline replay. It exposes an immutable `RenderFrame`
with retained GPU buffers and allocation metadata. The Editor retains the frame
and copies the camera values, then records its own opaque graphics pass after
the material system renders tubes. It uses the preview's color and depth targets
and preserves depth so leaves occlude and are occluded by the tubes correctly. The default
Engine hook does nothing. Its out-of-line virtual symbol is exported through
`Manager`'s existing `ELIXIR_API` for Windows.

The renderer reads persistent positions, transported normals, active lengths, and
accumulated development distance directly on the GPU. It does not read them back,
change solver state, or create a separate simulation. It draws the main chains and
branches, retaining the prior lobed/curved leaf geometry and procedural veins.
Each leaf uses 444 triangle-list vertices. The tube and leaf draws are independent.
Leaf size follows instance scale and particle scale.

`leafRenderer` is a sibling of `strand` on an emitter, and is parsed by the Editor.
The Engine ignores application renderer properties. An Engine-only application
therefore renders the same simulation as plain tubes unless it supplies its own
renderer. Add, remove, or disable **Leaf Renderer (Editor)** in the VFX emitter
inspector; these changes support undo, save, and preview recompilation. Missing
settings draw no leaves. Parsing invalid settings preserves the previous preview.

```json
"leafRenderer": {
  "enabled": true,
  "interval": 3,
  "length": 0.12,
  "width": 0.1,
  "color": [0.12, 0.45, 0.08, 1],
  "shape": "Ivy",
  "maturationSegments": 2
}
```

Interval is an integer from 1 through 96. Length, width, and maturation distance
must be finite and positive. RGB is nonnegative and alpha ranges from zero to one.
`shape` accepts `Ivy` or `Lanceolate`. Leaves are placed strictly between root and
last point: `floor((points - 2) / interval)`. They open after the tip reaches each
node, using `maturationSegments` of development-front progress. Pausing development
holds their opening while physical movement continues. This first custom renderer
uses opaque shading; alpha zero hides leaves, and partial alpha is not blended.
Its shaders are built and packaged only with the Editor in `EditorShaders/`.

Samples have moved all former `strand.leaf*` and growth-module leaf controls into
this block. Custom assets using the previous experimental format need the same
migration. Leaf graphics push constants occupy 52 bytes; tube constants occupy
64 bytes and strand compute constants remain 128 bytes.

## Growth and branching

| Field in `strand` | Default | Meaning |
| --- | --- | --- |
| `growthSpeed` | `0` | Nonnegative tip speed in simulation units per second. Zero starts fully grown. |
| `growthMaxLength` | `0` | Nonnegative maximum main-chain length. Zero uses `(points - 1) * segmentLength`; larger values are capped at that capacity. |
| `branchCount` | `0` | Side chains per main chain, from zero to four. Branches do not produce further branches. |
| `branchInterval` | `8` | Positive control-segment interval between attachment points. `branchCount * branchInterval` must remain smaller than `points - 1`. |
| `branchLengthScale` | `0.5` | Branch length and speed multiplier, greater than zero and at most one. |

Growth uses particle age rather than normalized lifetime. The solver extends one
segment at a time, with a partially grown segment at the tip. Unused points stay
collapsed at the tip, receive no point-force updates, and produce no visible
geometry. Existing grown points retain their Verlet state. Leaves open over the
time needed to grow two segments and continue maturing after the stem stops.

A branch waits until the main tip passes its attachment by one segment. Its root
then follows that simulated parent point, and its own chain responds to the same
point-force modules. Branches whose attachment lies beyond `growthMaxLength`
remain dormant. A branch shares its root particle's color, scale, death policy,
and spawn generation. Removing or killing that particle also removes its branches.

The ivy sample grows at 0.18 units per second up to 1.65 units. Three branches
attach at points 8, 16, and 24, grow at 45% of the main tip speed, and stop at
45% of the main length. The main stem matures in about 9.2 seconds and all branches
finish growing in about 15.4 seconds. Select **Ivy** again to restart growth while
preserving the instance position and camera. The continuous Strands panel shows
the full evolution; the five-second VFX preview shows only its initial stages.

Point storage per root is `points * (1 + branchCount)`. Branches reserve bounded
GPU point ranges and render as additional instances of the same draw. Default
growth and branch settings preserve existing strand assets. Mesh collisions,
self-collisions, recursive branching, and bending/twist constraints remain unsupported.

## Climbing ivy on static surfaces

Select **Climbing Ivy** in the Strands panel. This separate sample preserves
**Ivy** as free-hanging vegetation. Climbing Ivy starts at the bottom of a visible
wall and grows upward, following a plane, two spheres, and two rounded boxes
whose dimensions match the playground meshes. Use WASD to move the root; points
already attached stay fixed in world coordinates. Select the sample again to
restart and release its attachments.

The optional `strand.surfaceAttachment` object configures guidance:

| Field | Default | Meaning |
| --- | --- | --- |
| `surfaces` | Required array | Up to eight static analytic shapes. An empty array disables guidance and attachment. |
| `searchRadius` | `0.1` | Positive world-space distance at which free points start following a nearby surface. |
| `offset` | `0.014` | Nonnegative world-space clearance, smaller than `searchRadius`. |
| `climbDirection` | `[0, 1, 0]` | Nonzero preferred direction in world coordinates, projected into the surface tangent plane. |
| `climbStrength` | `4` | Nonnegative acceleration along that tangent, in world units per second squared. |

Every surface has a `type` and a three-component world-space `center`:

- `Plane` requires a nonzero `normal`. It defines an infinite plane through `center`.
- `Sphere` requires a positive `radius`.
- `Box` requires positive `halfExtents`. Optional `rounding` is from zero through
  the smallest half extent. Boxes are axis-aligned.

The GPU finds the nearest surface within the search radius. Solid penetration
takes priority over a nearby plane so a vine on a wall can turn onto an object.
Free points project to the surface plus clearance, and guidance steers the growing
tip along the surface tangent. A completed segment attaches after the tip passes
it by half a segment. Attached points ignore forces and remain fixed during
distance-constraint iterations. Branch chains use the same guidance and can
attach independently.

The existing three-vector point layout is preserved: previous-position `w` holds
the attachment index for non-root points; the root retains its generation marker.
A non-root attachment's world coordinate is retained in previous-position xyz.
The root's previous-position xyz retains the selected tip-guidance direction for
debug rendering. Respawn
and a fresh simulation generation clear attachment state. A separate 416-byte
surface block per emitter keeps the compute push constants at 128 bytes.

These are static strand-to-surface constraints, not general collision detection.
Moving surfaces and arbitrary mesh queries are unsupported. Attachment release
can be enabled with `breakStretchRatio`; zero retains permanent fixation. Edit
surface definitions and recompile to rebuild the simulation.

Run `python3 Assets/Meshes/StrandScene/generate.py --wall` to regenerate the visible
wall. Its front face is at z = -0.08, matching the sample's analytic plane.

## Release, tip guidance, and debug view

`strand.surfaceAttachment.breakStretchRatio` defaults to zero, which disables
release and preserves existing effects. A nonzero value must be finite and greater
than one. Climbing Ivy uses 2.5: an attachment releases when its neighboring segment
or its span from the previous remaining pin exceeds 2.5 times the corresponding
rest length. Checking spans allows release to propagate through free points when
pulling the root. The segment and plant remain alive; only the surface pin releases.

Released points respond to forces and distance constraints again. A 0.5-second
interval prevents immediate reattachment to the same nearby surface. It advances
only with simulated time and resets on respawn or a fresh generation. During this
interval, non-root previous-position w stores the negative remaining time instead
of a positive attachment marker.

Guidance combines the preferred climb direction with the previous completed
segment's heading. It preserves forward travel at a pole or corner instead of
reversing to chase the preferred direction. At the tip, it compares forward and
two lateral candidates, queries surfaces two segment lengths ahead, and penalizes
paths close to older control points. This is a bounded steering heuristic; it does
not provide self-collision or guarantee escape from every enclosed corner.

The Strands panel's **Debug: off/on** button displays geometry directly from the
GPU simulation buffers without restarting the instance:

- Green crosses: points attached to a surface.
- Orange crosses: released points waiting to reattach.
- Red crosses: points free of surface attachment, including the particle root.
- Yellow arrows: analytic surface normals.
- Blue arrows: the tip guidance selected by the solver.

Debug markers are two-sided and use solid colors independent of the asset's
material graph. They use the same depth-tested strand draw, with no GPU readback.
Turning debug off removes the extra draw vertices. `Manager::SetStrandDebug()` is
an exported Engine API and must run on the render-frame thread; the Editor queues
the change through `RunRenderTaskAndWait()`. The surface-block stride remains
416 bytes and compute push constants remain 128 bytes.

## Visual validation of the refined leaf

The updated Editor bundle was inspected in Climbing Ivy at the default camera
and at close zoom. The five-lobed silhouette, curved shading, short petiole, and
veins remained visible during simulation. Growth was restarted to inspect its
early stage. With Debug enabled, holding D moved the root to x = 0.80 and holding
A returned it near x = -0.02. Upper attached sections stayed in place while
released lower sections moved with the root; nearby points attached again.
The observed window stayed around 60–61 FPS during this local run. This is a
visual smoke check, not a performance benchmark or proof against every corner
case. CPU tests and GPU growth, contact, release, and debug rendering tests passed.

## Strand Growth module and plant samples

`StrandGrowth` is an Update module targeting `StrandPoints`. It configures a
separate once-per-chain development stage rather than emitting a force instruction
for every point. The solver consumes its active chain lengths and attachments,
while gravity and curl remain ordinary point-force modules. Only one growth module
may exist per emitter, and it requires at least three control points. Invalid
compilation is rejected before an incomplete emitter layout can be registered.

```json
{"type":"StrandGrowth", "target":"StrandPoints", "speed":0.18,
 "maxLength":1.65, "branchCount":3, "branchInterval":8,
 "branchLengthScale":0.45,
 "branchVariation":0.5, "branchDelay":0.6, "seed":123, "state":"Growing"}
```

The speed, length, branch count, nominal branch interval, and branch scale replace
their corresponding development fields in `strand`. Geometry resolution, stiffness,
damping, and surface constraints stay in `strand`. Leaf appearance and opening
settings belong to the Editor's `leafRenderer` block. Existing files without a growth module preserve
their original particle-age timing. Ivy samples have been migrated to an explicit
module with zero variation and delay to retain their existing development pattern.

Module controls use authored literals in this version:

- `speed` and `maxLength` are nonnegative and use the same units as legacy growth.
- `branchCount` is from zero to four. Branch nodes must fit before the configured tip.
- `branchVariation` is from zero to one and jitters branch nodes by at most half
  their interval, preserving ordering and valid point indices.
- `branchDelay` is a nonnegative maximum extra birth delay, in seconds.
- `seed` is an unsigned 32-bit value; per-root variation uses integer hashing.
- `state` is `Growing`, `Mature`, or `Paused`. Mature starts the complete plant;
  Paused holds its development clock, including branch births and leaf maturation.

The clock belongs to the main chain and is stored in point two's normal w. It is
independent of particle age and resets with spawn/generation state. Branch clocks
are relative to their seeded birth times. `Manager::SetStrandGrowthPaused()` pauses
and resumes all explicit growth modules in that manager without stopping physics
or restarting instances. The Editor's **Pause growth / Resume growth** button
uses this API. It has no effect on legacy strands without a growth module. An
authored `Paused` state remains paused until its module configuration changes;
the button controls the additional runtime pause.

A separate 32-byte development buffer per emitter keeps force bytecode and compute
push constants unchanged. The new Manager and Emitter out-of-line methods use
their existing `ELIXIR_API` class exports for Windows DLL linking.

Two additional samples appear in the Strands panel and Content Browser:

| Sample | Development | Appearance |
| --- | --- | --- |
| `StrandFern.json` / Fern | Seven stems, 0.22 units/s, 1.3-unit limit, four side branches with seeded node jitter and up to 0.6-second delay; leaves open over 1.5 segment intervals. | An upward-growing fern-like clump with dense narrow leaves and mild wind. |
| `StrandWillow.json` / Willow | Five stems, 0.12 units/s, 1.7-unit limit, three longer side branches with stronger node jitter and up to 1.6-second delay; leaves open over four segment intervals. | A hanging willow-like canopy with long narrow leaves, gravity and slow wind. |

These are procedural plant approximations, not complete botanical tree models.
Both samples omit surface constraints. `leafRenderer.shape` accepts `Ivy` (default)
or `Lanceolate`; the latter uses a narrow blade and central vein with the same
444-vertex leaf layout. All plant samples retain a single initial burst and
disable death by lifetime. Select a sample again to restart its development.

Fern and Willow were inspected in the Editor after compilation. Fern's denser,
shorter leaves and upward development differed from Willow's longer narrow leaves
and hanging stems. The growth button switched between pause and resume without
restarting the instance. Automated tests verified clock preservation while paused,
continued root motion, resumption without age catch-up, forced maturity, duplicate
module rejection, and rendering all samples with debug enabled and disabled.

## Tube width profile

Tube appearance belongs to the emitter's `strand` rendering properties, not the
`StrandGrowth` or force modules. The VFX emitter inspector shows these fields even
when an older asset omits them. Editing a missing field adds it to the asset and
supports undo.

```json
"strand": {
  "widthScale": 0.3,
  "rootWidthScale": 1.0,
  "tipWidthScale": 0.0,
  "widthExponent": 1.0,
  "branchWidthScale": 0.65
}
```

For normalized distance `u` along the currently grown chain, the radius multiplier
is `lerp(rootWidthScale, tipWidthScale, pow(u, widthExponent))`. Root and tip values
must be finite and nonnegative; the exponent must be finite and positive. One
gives a linear transition, larger exponents keep the root width farther along the
chain, and smaller exponents approach the tip width earlier. Equal endpoint values
give a uniform tube. Zero closes an endpoint. Nonzero endpoints leave the tube open;
this renderer does not generate end caps. Branches reuse the same profile with the
additional nonnegative `branchWidthScale` multiplier. This changes only geometry,
not segment length, physics, attachment, or growth.

The old fixed particlez taper is replaced by this profile. Assets without these
fields use a full-width root, zero-width tip and linear taper. The cables sample
uses equal endpoint widths; plant samples use a small nonzero tip and exponent 0.8.
The generic tube graphics push constants are 64 bytes in both shader stages and the C++ renderer,
within Vulkan's required minimum capacity of 128 bytes. No DLL API was added.

## Fixed-end cables and utility poles

The **Power Lines** sample has three static utility poles and three conductors over
each of two spans (six strands). Each emitter starts one persistent root, reserves
40 points, and fixes its last point at the next pole. A 1.17-unit rest length spans
1.05 units horizontally, leaving slack for gravity to form the sag. Both radius
multipliers are one, so the wires have uniform thickness. Curl noise provides wind;
no pole collision or leaf renderer is used.

A second pin is generic strand simulation data, independent of rendering:

```json
"strand": {
  "endAttachment": {
    "enabled": true,
    "position": [1.05, 0.69, 0.16],
    "space": "Simulation"
  }
}
```

The object is absent by default. Its presence enables the pin unless `enabled` is
false; enabled pins require a finite three-component position. `Simulation` uses
the emitter's simulation space: a Local emitter transforms the target with the
instance, while a World emitter uses it directly. `World` always treats the target
as an absolute world coordinate, independently of root movement. All main chains
in an emitter share this destination; use separate emitters for distinct targets.
Branches retain free tips.

The solver initializes a spanning curve with downward slack. It excludes the
endpoint from force integration and gives both pinned points zero inverse mass
in the distance constraints. Pins update even when the timestep is zero, including
the endpoint's history, so changing a target during pause introduces no stored
endpoint velocity. The interior continues Verlet/PBD physics. A pin stays exact
when its span exceeds the configured rest length, but the chain must then stretch;
use enough total segment length for the intended span and sag.

This stage requires fully grown chains: nonzero `growthSpeed` or `growthMaxLength`
is rejected when the pin is enabled, including values compiled from StrandGrowth.
The Inspector exposes **Strand End Attachment / Pin end**, destination, and space.
A separate 16-byte buffer per emitter carries targets at binding 10; compute and
graphics push-constant layouts remain unchanged. Debug shows both pins in green.
The sample keeps its poles and roots stationary, so WASD translation is disabled
there; orbit and zoom remain available. Other samples keep their movement controls.

GPU validation checks sag, segment stability, exact pins under forces, translation,
rotation and scale of simulation-space targets, independent world targets, paused
target movement, and respawn. The sample also runs through Aether's normal draw path.
