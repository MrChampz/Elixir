# GPU strands in Aether

`EParticleRenderMode::Strand` creates one persistent, simulated tube per live particle.
The particle is the root. Spawn and update modules still control its position, lifetime,
size, scale, and color. A separate compute pass moves the remaining control points
with Verlet integration, curl forces, and iterative distance constraints.

Open `Assets/VFX/GPUStrands.json` in the VFX editor for a working example. Select the
Strand render mode with the existing mode buttons. Edit the `strand` object in the
asset to configure point count, segment length, stiffness, damping, gravity, sway,
width, solver iterations, radial sides, and spline subdivisions. Settings are
validated before compilation. They are literal values in this first stage.

The sample emits 64 roots once, using `"spawnRate": 0` and
`"burst": {"count": 64, "interval": 0}`. A zero burst interval means one initial
burst per instance start or reset. `"particleState": {"killOnLifetimeExpired": false}`
keeps the roots alive indefinitely. This avoids both lifetime-based death and circular
buffer replacement during normal preview playback. Positive burst intervals retain
periodic emission; trigger-driven emitters still wait for their trigger events.

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

There are no collisions, self-collisions, or dedicated strand controls in the editor.
The solver accepts 2–96 points and 1–16 constraint iterations. It runs internal steps
of at most 1/60 second and caps simulated time at 8/60 second per submitted step to
avoid unstable jumps after long frame stalls. Lower stiffness permits stretching.
The current solver processes each chain sequentially in one GPU invocation.
