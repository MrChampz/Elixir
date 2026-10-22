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

Both characters use the same synchronized locomotion blend space and an `UpperBody`
layer selecting `spine` and its descendants. Local leg transforms continue to come
from locomotion. Press `J` to wave, `K` to punch, and `L` to blend the action layer out
or in over 0.25 seconds on both characters. Action switches crossfade over 0.15 seconds.
The character controller's UpperBody state machine plays a punch once and returns
to waving automatically. Press J during a punch to interrupt it and return early.
The HUD shows WAVE or PUNCH independently of the locomotion phase.
The right character also adds recoil, described below. `P`, `N`, and `F` retain pause,
frame-step, and silent base-contact seek. The HUD counts layer notifies separately
from the base footstep events. A muted layer continues its clock without emitting
notifies. Action event times are demonstration annotations, not gameplay hit windows.


Playback and composition settings now belong to `AnimationLayer`. Every mesh instance
has a permanent first layer named `Base`, with a full-body mask and weight one.
`GetBaseLayer()` and `GetLayer(name)` expose the same playback API. The instance
advances and composes every layer in one ordered pass through `Update`.
`Play`, `Seek`, and time getters on the instance remain shortcuts for `Base`.
`ClearLayers` and `Stop` preserve the base layer object. Notify events always carry
the owning layer's name, including `Base` for locomotion events.

## Additive recoil comparison

Both characters now run the same locomotion and upper-body action. The right character
adds a procedural `RecoilPulse` sequence, authored around the mesh's chest reference
transform. This additional clip is generated at runtime; the original KayKit clips
and the GLB remain unchanged. The chest mask preserves leg animation. Unanimated local
channels match the mesh reference, so their additive difference is zero.

`U` toggles recoil, `I` selects strength 0.5, and `O` selects strength 1. Both characters
still respond to `J`, `K`, and `L` together. At recoil weight zero their poses should
match. Recoil loops every 0.65 seconds; its peak at 0.08 seconds emits a `RecoilPeak`
notify counted as a shot on the HUD. Muting recoil keeps its clock moving and silences
its events. Pause and frame stepping affect all layers.

Layers default to `EAnimationLayerBlendMode::Override`. Select `Additive` with
`SetBlendMode`; Base always remains Override. The mesh reference is the default
additive origin. `SetAdditiveReferencePose` can instead use a full local pose sampled
from a chosen reference frame; an empty span restores the mesh reference.

For each selected bone, with effective weight `layerWeight * maskWeight`:

- Add `(sampleTranslation - referenceTranslation) * weight` to the underlying position.
- Pre-multiply the underlying local rotation by the weighted rotation difference
  `sampleRotation * inverse(referenceRotation)`, interpolated from identity by slerp.
- Multiply the underlying scale by `lerp(1, sampleScale / referenceScale, weight)`.

Composition follows layer order and runs in local transform space. Clips and blend
spaces continue to supply absolute poses; the layer calculates differences against
its reference after sampling and transitions. Referenced transforms must be valid,
including nonzero scale. A singular or nonfinite composed pose rejects the whole
instance update without advancing any layer or replacing its previous event batch.


## One controller per character

`AnimationController` now describes a complete character: shared typed parameters
and ordered `SAnimationControllerLayer` definitions. Each layer contains its own
`SAnimationStateMachine`, mask, initial weight, blend mode, and optional additive
reference pose. State names are local to each machine; parameters are shared across
all machines. Base must come first, use Override, and have weight one without a mask.

`AnimationControllerInstance(controller, mesh)` stores each character's parameter
values and one current state per layer. The first update creates the declared layers
on a mesh that has only Base, then starts every machine. One definition is shared by
both demo characters; the comparison character mutes its Recoil layer after startup.
No external controller per layer or separate mesh update is needed.

```cpp
AnimationControllerInstance animation(characterDefinition, mesh);
animation.SetFloat("Forward", speed);
animation.SetBool("Punch", punchRequested);
if (animation.Update(deltaSeconds))
{
    // Consume mesh.GetNotifyEvents() and render the updated mesh.
}
```

`Update(dt)` evaluates at most one transition per machine, then advances all layer
clocks and composes the mesh once. `GetCurrentState()` queries Base;
`GetCurrentState("UpperBody")` queries the action machine. Completion checks use the
corresponding layer's playback and fade. Override, additive, sequence, and blend-space
states follow the same update path. The demo declares locomotion, UpperBody actions,
and additive Recoil together and uses one controller instance per character.

`Reset()` restores shared parameter defaults and restarts all machines on the next
update, preserving runtime layer weights, masks, modes, and individual pause settings
until playback restarts. Use one active controller per mesh and drive its playback
and frame updates through that controller. The mesh must outlive it. Removing a
configured layer makes updates fail before evaluating any machine; construct a new
controller on a fresh mesh to configure a new layer layout. Invalid delta times do
not create layers or change playback. Failed composition preserves all playback
clocks, the previous composed pose, and the previous notify batch; chosen target
states remain selected so a later valid update can retry their transitions.
