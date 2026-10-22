# KayKit locomotion demo

Author: Kay Lousberg. License: CC0 (see LICENSE.txt).

Official source: https://kaylousberg.itch.io/kaykit-character-animations

Free 1.1 archive mirror: https://github.com/MightyBubble/Ludots-Assets/tree/main/animations

Archive SHA-256: `65882f31f905ad2e953819648a59287cdeab8f623908d5ef701971d3758be20f`

The GLB contains the original Rig_Medium mannequin and ten unmodified clips. Only animation references and binary offsets were merged. Reproduce the original six locomotion clips with:

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
from locomotion. Press `K` to play a punch montage and `L` to blend the action layer
out or in over 0.25 seconds on both characters. The UpperBody layer continuously
plays Waving underneath the punch. The montage fades in over 0.1 seconds and fades
out over its final 0.2 seconds, returning to the continuing wave. Press `J` to cancel
the punch early. No action states or transition parameters are needed.
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
and ordered `SAnimationControllerLayer` definitions. Each layer selects exactly one
`Sequence`, `BlendSpace`, or `StateMachine`, alongside its mask, initial weight, blend
mode, and optional additive reference pose. State names are local to each machine;
parameters are shared across every layer. Base must come first, use Override, and
have weight one without a mask.

`AnimationControllerInstance(controller, mesh)` stores each character's parameter
values and playback progress for every layer source. The first update creates the
declared layers on a mesh that has only Base, then starts their animation sources. One definition is shared by
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
clocks and composes the mesh once. `GetCurrentState(layerName)` queries the selected
machine's target state; direct sources have no named state and return an empty view.
`GetCurrentState("UpperBody")` queries the demo's action machine. Completion checks use the
corresponding layer's playback and fade. Override, additive, sequence, and blend-space
states follow the same update path. The demo's Base combines the locomotion BlendSpace
and dodge sequences through a machine. UpperBody retains its Wave/Punch machine and
Recoil plays its Sequence directly. They share one controller instance per character.

A direct blend-space layer needs only:

```cpp
{
    .BlendSpace = blendSpace,
    .BlendParameters = { "Strafe", "Forward" }
}
```

A direct sequence layer uses `.Sequence = animation`. Set `.Looping = false` to play
it once and hold its final pose. BlendParameters binds direct blend-space axes to
shared numeric parameters; count and order must match the source dimensions. In a
machine layer, configure blend parameters and looping inside each state. Empty or
ambiguous sources are rejected when creating the controller.

`Reset()` restores shared parameter defaults and restarts every layer source on the next
update, preserving runtime layer weights, masks, modes, and individual pause settings
until playback restarts. Use one active controller per mesh and drive its playback
and frame updates through that controller. The mesh must outlive it. Removing a
configured layer makes updates fail before evaluating any machine; construct a new
controller on a fresh mesh to configure a new layer layout. Invalid delta times do
not create layers or change playback. Failed composition preserves all playback
clocks, the previous composed pose, and the previous notify batch; chosen target
states remain selected so a later valid update can retry their transitions.


Direct sources compile to `SSequenceSource` or `SBlendSpaceSource`. The sequence
alternative contains its shared clip and looping flag. The blend-space alternative
contains its shared asset, resolved numeric bindings, and looping flag.
`TLayerSource` selects one of these two types or `SStateMachine`; each machine state's
`TPlaybackSource` selects a sequence or blend space. A state machine compiles to its own initial state,
per-state playback sources, and outgoing transitions. No state or machine is
constructed for a direct source. Runtime startup tracking for direct playback is
separate from the selected state of a machine. Both paths share source validation
and playback, and the mesh still advances and composes all layers only once.
Machines with one state and no transitions are valid when that structure is useful.


## Root motion

The GLB also includes the original `Dodge_Left` and `Dodge_Right` clips from
`Rig_Medium_MovementAdvanced.glb` in the same CC0 archive. The original node hierarchy,
reference transforms, and keyframes are preserved. These clips translate the root
approximately +0.5 and -0.5 units along X. Existing preparation scripts do not merge
these additional clips.

Press `B` to play the left dodge once as a Base montage, then return to locomotion.
Base uses the locomotion blend space directly, and keeps its phase and coordinates
advancing underneath the dodge. Entrance and exit take 0.05 seconds each. The
upper-body action fades out during the dodge. Both characters extract and lock the root; the left
character deliberately ignores the extracted delta, while the right applies it to
its scene transform. `V` resets the right character's position. `Root` in the HUD
shows its accumulated displacement. Recoil, pause, and frame stepping still work.

Root extraction is opt-in per mesh instance:

```cpp
mesh.SetRootMotionEnabled(true, "root");
if (animation.Update(deltaSeconds))
    characterTransform *= mesh.GetRootMotionDelta().ToMatrix();
```

Only Base contributes root motion. The selected bone's translation and rotation are
locked to the mesh reference before upper layers compose. An empty bone name selects
bone zero. Imported armature wrappers can precede the animated root; this demo selects
`root` below `Rig_Medium`. Ancestors must remain in their reference pose and use
uniform scale. Their reference transforms convert the delta into mesh space. Animated scale stays in the pose.
The returned rigid delta is in mesh-local space and is post-multiplied by gameplay;
the renderer never moves the character automatically. A successful Update produces
one delta, including identity when paused, disabled, completed, or given zero time.
Consume it once and only after successful updates. Seek, Play, Stop, manual pose
changes, and extraction toggles clear the mesh's last delta.

Sequence extraction accumulates complete rigid cycles, including turning cycles,
instead of subtracting wrapped positions. Blend spaces combine per-clip deltas using
the current weights and continuous marker-mapped phases. Changing coordinates does
not introduce movement from a change of absolute pose. During a fade, source and
target deltas use the average target weight over the update, including time after
the fade completes. Interrupted fade snapshots contribute stationary movement.
This is weighted interval blending, not exact integration of varying velocities
within a frame; nonlinear clips and rotation blends can vary with coarse frame steps.
At most 4096 cycles may be extracted in one interval. Invalid or nonfinite extraction
rejects the whole mesh update, preserving all clocks, the last pose, events, and delta.

Collision, character movement constraints, replication, and root motion from nonbase
layers are left to a later gameplay integration.


## One-shot montages

An `AnimationMontage` is an immutable sequence action with entrance and exit times
and an interruption policy. Each target `AnimationLayer` owns one independent active
montage; the same asset can play on many characters. It uses the layer's mask and
composition mode. Sections, queues, play rates, and combo branching are not included.

```cpp
const auto attack = AnimationMontage::Create({
    .Name = "Attack",
    .Sequence = attackSequence,
    .BlendIn = 0.1f,
    .BlendOut = 0.2f
});
mesh.GetLayer("UpperBody")->PlayMontage(attack);
// The controller still updates the character once per frame.
animation.Update(deltaSeconds);
// Optional early exit; no argument uses the asset's BlendOut.
mesh.GetLayer("UpperBody")->StopMontage();
```

`Play` and controller transitions change the underlying source while the montage
continues. `GetTime`, `GetNormalizedTime`, and `IsTransitioning` describe that source;
`GetMontage`, `GetMontageTime`, and `GetMontageWeight` describe the action. The action
automatically releases at the clip end. Overlapping entrance and exit durations use
the smaller envelope, so a very short clip may never reach full weight. Zero exit
duration switches immediately. Replacement snapshots the visible pose for its
entrance. An uninterruptible action rejects replacement and StopMontage; explicit
Stop or SetLocalPose still resets all playback. Pause freezes both clocks.

Only montage notifies emit while the action owns the layer, including fades. If an
update crosses its end, source events resume for the remaining interval with frame-
relative offsets. Muted layers suppress all events. Events retain their sequence,
typed payload, and layer name. Invalid sampling or composition preserves all clocks,
poses, events, and the last root delta. Consume outputs only after successful Update.

A montage on Base participates in existing root extraction: blend source and action
movement using the average envelope weight, then extract the remaining source interval
if the action ends inside the frame. As with playback fades, this is weighted interval
blending rather than exact integration of nonlinear velocities or rotations. A montage
on an upper layer changes the pose without contributing character root motion. The
existing compute skinning pass consumes the final composed pose without another pass.
