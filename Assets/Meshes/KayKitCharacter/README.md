# KayKit locomotion demo

Author: Kay Lousberg. License: CC0 (see LICENSE.txt).

Official source: https://kaylousberg.itch.io/kaykit-character-animations

Free 1.1 archive mirror: https://github.com/MightyBubble/Ludots-Assets/tree/main/animations

Archive SHA-256: `65882f31f905ad2e953819648a59287cdeab8f623908d5ef701971d3758be20f`

The GLB contains the original Rig_Medium mannequin and eight unmodified clips. Only animation references and binary offsets were merged. Reproduce the original six locomotion clips with:

```sh
python3 Dissolve/Tools/PrepareBlendSpaceModel.py KayKit-Character-Animations-Free-1.1.zip Assets/Meshes/KayKitCharacter
```

Axes: X = strafe, Y = forward. Samples: Idle_A (0,0), Walking_A (0,1.5), Running_A (0,3), Walking_Backwards (0,-1.5), Running_Strafe_Left (-3,0), Running_Strafe_Right (3,0).
These clips have different gaits. Preparation preserves their original timing and does not align foot contacts; intermediate poses may show foot sliding.


The layer demo also includes `Waving` from `Rig_Medium_Simulation.glb` and
`Melee_Unarmed_Attack_Punch_A` from `Rig_Medium_CombatMelee.glb`, from the same archive.
The original six-clip preparation script does not include these two action clips.
Their animation accessors, buffer views, and node references were merged by name after
checking identical reference transforms and hierarchy. The original keyframes remain
unchanged. No new external assets or license terms are required.

Both characters now use the same synchronized locomotion blend space. Only the right
character has an `UpperBody` layer selecting `spine` and its descendants. Local leg
transforms continue to come from locomotion. Press `J` to wave, `K` to punch, and `L`
to blend the layer out or in over 0.25 seconds. Action switches crossfade over 0.15
seconds. `P`, `N`, and `F` retain pause, frame-step, and silent base-contact seek.
The HUD counts layer notifies separately from the base footstep events. A muted layer
continues its clock without emitting notifies. Action event times are demonstration
annotations, not gameplay hit windows.


Playback and composition settings now belong to `AnimationLayer`. Every mesh instance
has a permanent first layer named `Base`, with a full-body mask and weight one.
`GetBaseLayer()` and `GetLayer(name)` expose the same playback API. The instance
advances and composes every layer in one ordered pass through `Update`.
`Play`, `Seek`, and time getters on the instance remain shortcuts for `Base`.
`ClearLayers` and `Stop` preserve the base layer object. Notify events always carry
the owning layer's name, including `Base` for locomotion events.
