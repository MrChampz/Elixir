# Growing particle chains

Strands use the same persistent CoreV1 particles as sprites and ribbons. A
`DistanceConstraint` connects particles with matching emitter and `GroupId`, consecutive
`LinkOrder` values separated by `orderStep`, and a maximum of `maxPoints` particles per
chain. There is no separate strand-point allocation or strand simulation target.

Open the Editor's **Strands** tab to select Ivy, Climbing Ivy, Fern, Willow, Power Lines,
or Balloons. The original four samples remain available. Drag to orbit, use the wheel to
zoom, and use WASD to move the instance. Power Lines stays fixed with its reference poles.
**Pause/resume growth** stops development clocks while particle physics continues.

The same JSON assets open in the VFX editor. Numeric and nested settings use its existing
property inspector, undo/redo, save, compile, and replay paths. The module palette includes
`StrandGrowth`, `DistanceConstraint`, and `SetGroupIdFromSpawnOrder`.

## Development and topology

Add one `StrandGrowth` update module and exactly one `DistanceConstraint`. The order of
these two modules does not matter. Growth is compiled into the distance constraint, after
ordinary particle operations and integration. It does not emit a second force program.

```json
{
  "type": "StrandGrowth",
  "speed": 0.18,
  "maxLength": 1.65,
  "branchCount": 3,
  "branchInterval": 8,
  "branchLengthScale": 0.45,
  "branchVariation": 0.4,
  "branchDelay": 0.6,
  "seed": 123,
  "state": "Growing"
}
```

Speed and length use simulation-space units. Zero speed starts fully developed. Zero
`maxLength` uses all `maxPoints - 1` segments. States are `Growing`, `Mature`, and `Paused`.
The runtime pause switch preserves accumulated development time. It does not change particle
age, lifetime, or velocity. A seeded branch starts after its parent reaches its attachment
point and its additional delay expires. `branchVariation` jitters attachment locations.

Reserve particles for every main chain and branch before spawning:

- `orderStep = mainChainCount * (1 + branchCount)`.
- `maxParticles = maxPoints * orderStep`.
- Set `strand.maxPoints` and `strand.orderStep` to match the distance constraint.
- Spawn the full capacity in one initial burst, with `spawnRate = 0`, burst interval zero,
  and `particleState.killOnLifetimeExpired = false`.
- Use `SetGroupIdFromSpawnOrder` with `groupCount = orderStep` and `firstGroupId = 0`.

Lanes are interleaved by point index. Each main lane is followed by its branch lanes.
Branches use existing particle slots and anchor to parent particles. One shader invocation
owns a main chain and all its branches; branches read their parent's completed positions.
The compiler rejects incomplete budgets, repeated growth modules, invalid branch topology,
expiring particles, and unsupported endpoint/growth combinations.

Development reserves `Transform.zw` in the existing CoreV1 particle ABI. On a root these
words hold accumulated development time and the developed segment count. On other particles
they hold segment visibility and distance behind the development front. Ordinary updates
preserve these words; spawning clears them. Tube and Editor attachment shaders consume
this metadata without changing `Alive`, `LinkOrder`, or particle age.

## Endpoints and surfaces

Place `endAttachment` inside the `DistanceConstraint` module:

```json
{
  "type": "DistanceConstraint",
  "maxPoints": 40,
  "orderStep": 1,
  "segmentLength": 0.03,
  "stiffness": 1,
  "pinFirstParticle": true,
  "endAttachment": {
    "enabled": true,
    "position": [0, 0.69, -0.16],
    "space": "World"
  }
}
```

`Simulation` follows the emitter's simulation space; `World` remains fixed as a local
instance moves. Only the complete main chain's last particle is pinned. Branch tips remain
free. Fixed endpoints require mature or zero-speed growth. Choose enough segment length to
span both endpoints; unreachable endpoints cannot satisfy every distance simultaneously.

`surfaceAttachment`, also inside `DistanceConstraint`, accepts up to eight static analytic
world surfaces. Shapes are `Plane` (`center`, `normal`), `Sphere` (`center`, `radius`), and
rounded axis-aligned `Box` (`center`, `halfExtents`, `rounding`). Climbing Ivy shows all three.
`searchRadius` and `offset` use world units. `climbDirection` is projected onto the surface;
`climbStrength` applies acceleration along that projection. A positive `breakStretchRatio`
greater than one suppresses attachment while a segment exceeds that ratio. Nearby particles
can attach again after the stretch decreases. This is analytic guidance, not mesh collision.
Singular or nonfinite instance transforms stop the affected constraint safely.

## Rendering and ownership

The `strand` object contains only tube geometry: topology, width scale, root/tip radius
multipliers, width exponent, branch width multiplier, radial segments, and subdivisions.
Taper is measured against the developed tip. Defaults keep existing tubes at constant width.

`leafRenderer` and `balloonRenderer` are Editor-owned settings, separate from simulation.
Leaves use an Ivy or Lanceolate silhouette and unfold behind the growth front. Balloons use
the moving tip of each main chain. Both use the existing particle buffer and the immutable
render item's transform, topology, and optional constraint snapshot. They render after
material geometry into the same preview color and depth attachments, before postprocessing.
Editor shaders are packaged only with the Editor.

Constraint uniforms use one persistent descriptor. Ordered transfer copies and barriers
supply each dispatch's values. Uploads remain alive until their graphics frame slot is safe
to reuse. This also supports multiple constraint modules and explicit preview replay steps.

## Validation

The unit tests cover parsing, validation, compiled particle budgets, immutable settings,
world transforms, and all six migrated assets. GPU tests cover interleaved chain solving,
branch anchors, independent development pause, fixed world endpoints, surface projection,
legacy multiple-constraint behavior, and development surviving normal particle updates.
The Editor's leaf and balloon setting tests cover invalid input and emitter indexing.
Windows-specific sources are not introduced. New out-of-line Engine APIs are exported through
`ELIXIR_API`; native Windows compilation and linking still require Windows CI.

Validation on macOS ARM64: Engine and Editor compiled and linked; all 950 tests without
GPU dependencies passed. Nine targeted Vulkan tests passed, including all six new assets,
legacy strands, branch development, pause, and plane/sphere/rounded-box attachments. Ivy,
Climbing Ivy, Power Lines, and Balloons were also inspected in the Editor. Run Vulkan tests
from `Debug/Build/Debug/macOS/arm64/Elixir.UnitTests` with `VULKAN_SDK` set; the runtime
folder contains both compiled shaders and the HLSL templates needed by material compilation.
