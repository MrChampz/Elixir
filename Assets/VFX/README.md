# Particle material examples

The examples migrated from `feature/particles-work` use the current material graph
compiler. Their visual settings are embedded in `material`, rather than stored in
emitter GPU state or sprite-specific push constants.

- `BlendModeTest`: translucent and additive blending.
- `FlipbookSmokeTest`: interpolated atlas animation over particle lifetime.
- `GradientTest`: luminance-based color ramp.
- `NormalLightTest`: directional shading from a normal texture.
- `DistortionTest`: screen-space refraction of the rendered background.
- `NS_Explosion_01` and `N_RealExp_00_A`: diffuse and emissive explosion atlases.
- `CinematicFX_Showcase`: layered effects using these material features.

## Embedded material properties

Existing properties (`color`, `emissive`, `texture`, and `blendMode`) remain valid.
`shadingModel` selects `Lit` or `Unlit`. Color and emission examples use `Unlit`;
`NormalLightTest` uses `Lit` to demonstrate the renderer light and normal channel.
`blendMode` also accepts `Additive`, which adds alpha-weighted source color and
preserves destination alpha. Additive items do not write depth.

```json
"material": {
  "texture": "Assets/Textures/T_SideExplosion_0_diffuse.png",
  "emissiveTexture": "Assets/Textures/T_SideExplosion_0_volemission.png",
  "emissiveScale": 2.0,
  "blendMode": "Translucent",
  "atlas": { "columns": 10, "rows": 10, "interpolate": true }
}
```

Check the atlas dimensions against the source image when authoring a new effect.
Cells are row-major from the top-left, starting at zero. Their count is derived
from `columns * rows`; the atlas resource has no separate frame count. The embedded
material advances across the whole grid over particle lifetime. Without interpolation,
every cell has an equal interval; with interpolation, playback blends from the first
frame to the last. Both diffuse and emissive textures share that frame. Atlas
sampling clamps to each cell's interior and uses mip level zero to avoid bleeding
across cells. Cell boundaries use `floor(cellIndex * imageExtent / cellCount)` in texels. Non-divisible grids
remain valid: neighboring cells can differ by one texel, and filtering stays
between the first and last texel centers of each cell.
Sprite UVs use a top-left origin for both coordinate channels. Regular textures
and atlases sample these same UVs without material-specific orientation changes.

`colorRampTexture` replaces diffuse RGB with a horizontal ramp indexed by diffuse
luminance, while preserving diffuse alpha and particle tint. `normalTexture`
feeds the material's tangent-space normal channel. Light direction, color, and
intensity come from the renderer's default Surface lighting setup. Materials do
not store scene-light or ambient-light settings. Particle shaders share Surface's
direct-light evaluation; Surface image-based lighting remains in its existing path.

`refractionTexture` supplies decoded normal-map XY offsets into the background.
`refractionStrength` scales those offsets in normalized screen coordinates
(default `0.02`). Refraction uses a radial opacity falloff and particle alpha.
Use translucent blending for this effect. The render target must support
`TransferSrc`, as it does for transmission materials.

`TextureAtlasSample` and `SceneColor` are generic graph nodes available to surface
materials and all particle geometries. `TextureAtlasSample` accepts an explicit frame
input, so it does not require particle timing. The node takes a `TextureAtlas`
resource parameter. The resource retains its texture and immutable layout; the
node selects frame interpolation and index or normalized frame coordinates.
To animate only part of a grid, use index mode and connect a graph expression to
`Frame`. For example, `ParticleTime.NormalizedAge * 9` selects frames 0 through 9
of a 4-by-3 atlas, leaving the last two cells unused. Normalized mode instead spans
the whole grid, so overriding the resource also updates that animation span.
Material instances can override the resource, including its layout, without
recompiling the graph. The embedded JSON atlas description remains an authoring
adapter that creates these resources. `ParticleTime` supplies particle
age, lifetime, or normalized age for particle pixel graphs.

## Burst timing

```json
"burst": { "count": 24, "interval": 0.0, "delay": 0.15 }
```

`delay` is an optional nonnegative initial delay, in seconds, for any emitter's
automatic bursts. With a zero interval, the burst runs once per instance generation
when the delay expires. Periodic bursts first run after `delay + interval` and then
every `interval`. Resetting the instance restarts this timing. Continuous spawn
rates are unaffected. Trigger-driven emitters continue to use `trigger.delay`.
