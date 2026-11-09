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
`blendMode` also accepts `Additive`, which adds alpha-weighted source color and
preserves destination alpha. Additive items do not write depth.

```json
"material": {
  "texture": "Assets/Textures/T_SideExplosion_0_diffuse.png",
  "emissiveTexture": "Assets/Textures/T_SideExplosion_0_volemission.png",
  "emissiveScale": 2.0,
  "blendMode": "Translucent",
  "atlas": { "columns": 8, "rows": 8, "frames": 64, "interpolate": true }
}
```

Check the atlas dimensions against the source image when authoring a new effect.
Frames are row-major from the top-left, starting at zero. The embedded material
advances from the first to the last frame over particle lifetime. Both diffuse
and emissive textures share that frame. Atlas sampling clamps to each cell's
interior and uses mip level zero to avoid bleeding across cells. Materials without
an atlas retain their existing texture sampling behavior.

`colorRampTexture` replaces diffuse RGB with a horizontal ramp indexed by diffuse
luminance, while preserving diffuse alpha and particle tint. `normalTexture`
modulates diffuse color using `normalLightDirection` (default `[-0.4, 0.6, 0.7]`)
and `ambientLight` (default `0.35`, valid range zero to one).

`refractionTexture` supplies decoded normal-map XY offsets into the background.
`refractionStrength` scales those offsets in normalized screen coordinates
(default `0.02`). Refraction uses a radial opacity falloff and particle alpha.
Use translucent blending for this effect. The render target must support
`TransferSrc`, as it does for transmission materials.

`FlipbookSample` and `SceneColor` are generic graph nodes available to surface
materials and all particle geometries. `FlipbookSample` accepts an explicit frame
input, so it does not require particle timing. `ParticleTime` supplies particle
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
