# KayKit locomotion demo

Author: Kay Lousberg. License: CC0 (see LICENSE.txt).

Official source: https://kaylousberg.itch.io/kaykit-character-animations

Free 1.1 archive mirror: https://github.com/MightyBubble/Ludots-Assets/tree/main/animations

Archive SHA-256: `65882f31f905ad2e953819648a59287cdeab8f623908d5ef701971d3758be20f`

The GLB contains the original `Rig_Medium` mannequin and these locomotion clips:

| Clip | Blend-space position (strafe, forward) |
| --- | --- |
| `Idle_A` | (0, 0) |
| `Walking_A` | (0, 1.5) |
| `Running_A` | (0, 3) |
| `Walking_Backwards` | (0, -1.5) |
| `Running_Strafe_Left` | (-3, 0) |
| `Running_Strafe_Right` | (3, 0) |

X is strafe and Y is forward. The demo adds `LeftFootPlant` and `RightFootPlant` sync
markers and `Footstep` notifies at runtime. The imported keyframes and their timing remain
unchanged. These clips have different gaits, so blending can still show foot sliding even
with synchronized contacts.

The GLB also includes `Waving` from `Rig_Medium_Simulation.glb`,
`Melee_Unarmed_Attack_Punch_A` from `Rig_Medium_CombatMelee.glb`, and `Dodge_Left` and
`Dodge_Right` from `Rig_Medium_MovementAdvanced.glb` in the same archive. Animation
accessors, buffer views, and node references were merged by name after checking the
reference transforms and hierarchy. No additional license terms are required.

## Demo controls

`Dissolve::InitializeAnimationGraph()` creates one shared graph and two independent
`Animation` objects. Its output is a `BlendNode` with three named children:

- `Locomotion`: the synchronized 2D blend space, bound to `Strafe` and `Forward`.
- `UpperBody`: looping `Waving`, masked to `spine` and its descendants. A punch montage
  temporarily overlays this continuing source.
- `Recoil`: an additive `RecoilPulse` sequence masked to `chest` and its descendants.

Both characters receive the same locomotion, punch, and dodge commands. The comparison
character mutes `Recoil` and ignores extracted root displacement. The other character uses
the selected recoil strength and applies its root delta to its scene transform.

| Input | Effect |
| --- | --- |
| `W`, `A`, `S`, `D` | Request directional walking while held. |
| `Shift` or `R` with movement | Request running instead of walking. |
| Arrow keys | Adjust the persistent strafe and forward blend coordinates. |
| `1`, `2`, `3` | Select coordinate presets (0, 2.25), (1, 1), or (-1.5, -0.75). |
| `Space` | Reset the persistent coordinates to idle. |
| `K` | Start `PunchCombo`, or queue Attack2/Attack3 from the current attack. |
| `J` | Jump an active punch montage directly to Recovery. |
| `L` | Fade the UpperBody contribution out or in over 0.25 seconds. |
| `B` | Play `Dodge_Left` once on Locomotion; fade UpperBody out during the dodge. |
| `V` | Reset the accumulated root-motion scene transform. |
| `U`, `I`, `O` | Toggle recoil, select strength 0.5, or select strength 1. |
| `P` | Toggle pause. |
| `N` | Pause and advance one 1/30-second animation step. |
| `F` | Pause and silently seek the base contact phase. |

The HUD shows phase, PLAYING/PAUSED, both footstep counts, WAVE or the current punch
section (ATTACK 1, ATTACK 2, ATTACK 3, RECOVERY), action-event count, recoil strength,
shot count, and accumulated root displacement. A second line shows Hitbox ON/OFF, window
Update count, and cancelled End count. Muted layers retain their clocks while suppressing
notifies. Action timings are demonstration annotations, not collision or damage queries.

## Additive recoil comparison

Both characters run the same locomotion and upper-body action. The recoil character adds a
procedural `RecoilPulse` sequence, authored around the mesh's chest reference transform.
This additional clip is generated at runtime; the original KayKit clips and the GLB remain
unchanged. The chest mask preserves leg animation. Unanimated local channels match the
mesh reference, so their additive difference is zero.

`U` toggles recoil, `I` selects strength 0.5, and `O` selects strength 1. Both characters
still respond to `J`, `K`, and `L` together. At recoil weight zero their poses should
match. Recoil loops every 0.65 seconds; its peak at 0.08 seconds emits a `RecoilPeak`
notify counted as a shot on the HUD. Muting recoil keeps its clock moving and silences its
events. Pause and frame stepping affect all layers.

Layers default to `EAnimationLayerBlendMode::Override`. Select `Additive` with
`SetBlendMode`; Base always remains Override. The animation reference is the default
additive origin. `SetAdditiveReferencePose` can instead use a full local pose sampled from
a chosen reference frame; an empty `SLocalPose` restores the animation reference pose.

For each selected bone, with effective weight `layerWeight * maskWeight`:

- Add `(sampleTranslation - referenceTranslation) * weight` to the underlying position.
- Pre-multiply the underlying local rotation by the weighted rotation difference
  `sampleRotation * inverse(referenceRotation)`, interpolated from identity by slerp.
- Multiply the underlying scale by `lerp(1, sampleScale / referenceScale, weight)`.

Composition follows layer order and runs in local transform space. Clips and blend spaces
continue to supply absolute poses; the layer calculates differences against its reference
after sampling and transitions. Referenced transforms must be valid, including nonzero
scale. A singular or nonfinite composed pose rejects the whole animation update without
advancing any layer or replacing its previous event batch.


## Shared graph, independent animations

`AnimationGraph` owns immutable parameter definitions and a connected output node.
`CreateAnimation(referencePose)` creates independent parameters, evaluators, clocks,
layers, and published frames for each character. `SkeletalMeshInstance` retains a fixed
mesh and its model-space reference pose; it does not advance animation, own layers,
extract movement, or publish notifies.

Use `SequenceNode::Create(sequence)`, `BlendSpaceNode::Create(blendSpace)`, and
`StateMachineNode::Create(machine)` to connect resources. A `BlendNode` composes a
full-body first input and ordered masked inputs. Each child specifies its own `Looping`
policy and numeric `InputParameters` bindings. The graph output has its own
`InputParameters` for a direct numeric source; a `BlendNode` instead binds its children.

The following setup assumes `mesh`, `blendSpace`, `waveAnimation`, and `recoilAnimation`
have already been loaded or created with compatible skeletons. The examples use the
`Elixir` namespace and the corresponding animation, mesh, and renderer headers.

```cpp
const auto upperBody = BoneMask::FromBone("UpperBody", mesh->GetSkeleton(), "spine");
const auto recoilMask = BoneMask::FromBone("Recoil", mesh->GetSkeleton(), "chest");
const auto graph = AnimationGraph::Create({
    .Name = "KayKitCharacter",
    .Parameters = {{"Strafe", 0.0f}, {"Forward", 0.0f}},
    .Output = BlendNode::Create("CharacterComposition", {
        {
            .Name = "Locomotion",
            .Node = BlendSpaceNode::Create(blendSpace),
            .InputParameters = {"Strafe", "Forward"}
        },
        {
            .Name = "UpperBody",
            .Mask = upperBody,
            .Node = SequenceNode::Create(waveAnimation)
        },
        {
            .Name = "Recoil",
            .Mask = recoilMask,
            .BlendMode = EAnimationLayerBlendMode::Additive,
            .Node = SequenceNode::Create(recoilAnimation)
        }
    })
});
if (!graph) return;

const auto meshInstance = SkeletalMeshInstance::Create(mesh);
const auto animation = graph->CreateAnimation(mesh->GetData().ReferencePose);
if (!meshInstance || !animation) return;
```

`AnimationLayer` owns composition settings such as mask, weight, blend mode, and additive
reference pose. Its `GetPlayer()` owns playback commands, time queries, transitions, and
montages. An `Animation` always retains an outer `Base` control with a full-body mask and
weight one. In this graph, Base plays the composition and exposes its `Locomotion`,
`UpperBody`, and `Recoil` children through `animation->GetLayer(name)`. Unknown or
ambiguous nested names return null. Explicit paths can disambiguate nested children within
a composition.

Update parameters through `animation->GetParameters()`, then call `Update(dt)` once per
frame. This evaluates the connected graph, composes the pose, and publishes model
matrices, notify events, and root motion together. Failed updates preserve the previous
published frame. Consume outputs only after successful updates, and supply the model pose
explicitly to the renderer between `BeginFrame` and `EndFrame`:

```cpp
if (!animation->GetParameters().SetFloat("Forward", speed)) return;
if (!animation->GetParameters().SetFloat("Strafe", strafe)) return;
if (animation->Update(deltaSeconds))
{
    renderer.Render(*meshInstance, animation->GetModelPose(), characterTransform);
    // Consume animation->GetNotifyEvents() once for this successful update.
}
```

The renderer overload without a model pose draws the mesh reference pose. Multiple
animations can use the same mesh, and a shared graph does not share mutable playback.
`Play`, `Seek`, and time getters on `Animation` refer to its Base playback.
`GetCurrentState(layerName)` queries a machine source; direct sequences and blend spaces
return an empty state name. This demo uses connected blend and sequence nodes, not
locomotion or Wave/Punch state machines. `ClearLayers` and `Stop` preserve the outer Base
control; changing its source can invalidate borrowed nested layer pointers.

## Root motion

The GLB also includes the original `Dodge_Left` and `Dodge_Right` clips from
`Rig_Medium_MovementAdvanced.glb` in the same CC0 archive. The original node hierarchy,
reference transforms, and keyframes are preserved. These clips translate the root
approximately +0.5 and -0.5 units along X.

Press `B` to play the left dodge once as a montage on the `Locomotion` child. Its
blend-space source keeps its phase and coordinates advancing underneath the dodge.
Entrance and exit take 0.05 seconds each. The upper-body action fades out during the
dodge. Both animations extract and lock the root; the comparison character ignores the
extracted delta, while the recoil character applies it to its scene transform. `V` resets
that accumulated transform. `Root` in the HUD shows its accumulated displacement. Recoil,
pause, and frame stepping still work.

Root extraction is opt-in per `Animation`:

```cpp
if (!animation->SetRootMotionEnabled(true, "root")) return;
// In the frame update, consume the delta after a successful Update.
if (animation->Update(deltaSeconds))
    characterTransform *= animation->GetRootMotionDelta().ToMatrix();
```

Root extraction follows Base and its primary source; the graph's first child is
Locomotion. UpperBody and Recoil do not contribute movement. The selected bone's
translation and rotation are locked to the animation reference pose. An empty bone name
selects bone zero. Imported armature wrappers can precede the animated root; this demo
selects `root` below `Rig_Medium`. Ancestors must remain in their reference pose and use
uniform scale. Their reference transforms convert the delta into mesh space. Animated
scale stays in the pose. The returned rigid delta is in mesh-local space and is
post-multiplied by gameplay; the renderer never moves the character automatically. A
successful Update produces one delta, including identity when paused, disabled, completed,
or given zero time. Consume it once and only after successful updates. Seek, Play, Stop,
manual pose changes, and extraction toggles clear the animation's last delta.

Sequence extraction accumulates complete rigid cycles, including turning cycles, instead
of subtracting wrapped positions. Blend spaces combine per-clip deltas using the current
weights and continuous marker-mapped phases. Changing coordinates does not introduce
movement from a change of absolute pose. During a fade, source and target deltas use the
average target weight over the update, including time after the fade completes.
Interrupted fade snapshots contribute stationary movement. This is weighted interval
blending, not exact integration of varying velocities within a frame; nonlinear clips and
rotation blends can vary with coarse frame steps. At most 4096 cycles may be extracted in
one interval. Invalid or nonfinite extraction rejects the whole animation update,
preserving clocks, the last pose, events, and delta.

Collision, character movement constraints, replication, and movement contributions from
secondary composition children are left to a later gameplay integration.


## One-shot montages

An `AnimationMontage` is an immutable sequence action with entrance and exit times and an
interruption policy. Each target layer's `AnimationPlayer` owns one independent active
montage; the same asset can play on many characters. It uses the layer's mask and
composition mode. Named sections provide timeline routing; independent montage queues and
play rates are not included.

```cpp
const auto attack = AnimationMontage::Create({
    .Name = "Attack",
    .Sequence = attackSequence,
    .BlendIn = 0.1f,
    .BlendOut = 0.2f
});
if (!attack) return;
auto* upperBody = animation->GetLayer("UpperBody");
if (!upperBody) return;
auto& player = upperBody->GetPlayer();
if (!player.PlayMontage(attack)) return;
// Continue calling animation->Update(deltaSeconds) once per frame.
// Optional early exit; no argument uses the asset's BlendOut.
player.StopMontage();
```

`AnimationPlayer::Play` and state-machine transitions change the source while a montage
continues. `GetTime`, `GetNormalizedTime`, and `IsTransitioning` describe that source;
`GetMontage`, `GetMontageTime`, and `GetMontageWeight` describe the action. The action
automatically releases at the clip end. Overlapping entrance and exit durations use the
smaller envelope, so a very short clip may never reach full weight. Zero exit duration
switches immediately. Replacement snapshots the visible pose for its entrance. An
uninterruptible action rejects replacement and StopMontage; explicit Stop or SetLocalPose
still resets all playback. Pause freezes both clocks.

Only montage notifies emit while the action owns the layer, including fades. If an update
crosses its end, source events resume for the remaining interval with frame- relative
offsets. Muted layers suppress all events. Events retain their sequence, typed payload,
and layer name. Invalid sampling or composition preserves all clocks, poses, events, and
the last root delta. Consume outputs only after successful Update.

A montage on Base or its primary Locomotion child participates in root extraction: blend
source and action movement using the average envelope weight, then extract the remaining
source interval if the action ends inside the frame. As with playback fades, this is
weighted interval blending rather than exact integration of nonlinear velocities or
rotations. A montage on an upper layer changes the pose without contributing character
root motion. The existing compute skinning pass consumes the final composed pose without
another pass.


## Montage sections and combos

The demo builds a separate `PunchCombo` timeline by sampling the original punch at 60 Hz
three times and holding the final punch pose for 0.3 seconds as Recovery. This runtime
demonstration does not change the GLB. Section boundaries are Attack1 at 0, Attack2 at one
punch duration, Attack3 at two durations, and Recovery at three durations. Each attack
defaults to Recovery; K updates the outgoing link on both demo animations without changing
the shared asset. The montage fades out throughout Recovery toward the live UpperBody
source. The punch starts and ends in the same pose, so the section jump remains
continuous. Recovery does not restart Waving at time zero; Waving keeps advancing
underneath. The HUD shows ATTACK 1, ATTACK 2, ATTACK 3, or RECOVERY, and each attack emits
PunchHit.

This example assumes one-second attacks and a 3.3-second timeline. The demo derives the
section boundaries from the imported punch duration.

```cpp
const auto combo = AnimationMontage::Create({
    .Name = "Combo",
    .Sequence = comboTimeline,
    .BlendIn = 0.1f,
    .BlendOut = 0.3f,
    .Sections = {
        { "Attack1", 0.0f, "Recovery" },
        { "Attack2", 1.0f, "Recovery" },
        { "Attack3", 2.0f, "Recovery" },
        { "Recovery", 3.0f, "" }
    }
});
if (!combo) return;
auto* upperBody = animation->GetLayer("UpperBody");
if (!upperBody) return;
auto& player = upperBody->GetPlayer();
if (!player.PlayMontage(combo)) return;
player.SetNextMontageSection("Attack1", "Attack2");
player.JumpToMontageSection("Recovery");
```

Sections cover the sequence in strictly increasing timeline order, starting at zero. Each
ends at the next start or clip end. Names are nonempty and unique. Omitted NextSection
follows timeline order; an empty string ends the action, and any existing name creates a
jump or loop. No authored sections creates one Default section, preserving earlier
playback behavior. `PlayMontage(combo, "Attack2")` starts a named section with a fresh
entrance clock. GetMontageTime reports sequence position; GetCurrentMontageSection reports
its section. The blend envelope uses elapsed action time independently of sequence jumps
and fades toward the end of the selected route. Loops do not automatically fade out. The
player's continuing source still advances.

SetNextMontageSection changes per-playback links without modifying the shared asset. An
empty destination ends at that section boundary. Changes during an exit, or too late to
preserve the current weight, fail without changing playback. StopMontage can fade a loop
out. Explicit section jumps are silent and immediate: no skipped events or root
displacement, no entrance-clock reset, and pause remains in effect. Start events emit on
the next positive update. Jumps and link changes remain available for noninterruptible
actions, whose policy rejects replacement and explicit stop; section jumps are rejected
once an explicit stop has begun.

Each update builds one list of visited forward intervals for pose position, events, and
movement. Adjacent sections emit a shared boundary event once. A nonadjacent link emits
outgoing end events before destination start events. Frame offsets remain chronological
through multiple section crossings; source events resume if the action ends inside the
frame. Root extraction composes only visited interval deltas, excluding absolute offsets
between sections. Traversals beyond 4096 intervals or combined events beyond the existing
capacity fail transactionally, preserving clocks, pose, events, and root delta. The Vulkan
readback test covers a section jump before compute skinning.


## Unified notifies and active windows

Each attack in the demonstration's runtime combo has an `AttackHitbox` window from 30% to
65% of that attack, with an application-defined `SAttackWindowPayload`. The HUD shows
Hitbox ON/OFF, Update count, and cancelled End count. This demonstrates event-driven
activation; no collision shape or damage query is created. Press K, then P during an
attack to inspect a window with N. J jumps to Recovery and must turn the indicator off
even while paused. L mutes the action layer and closes its active window when the weight
reaches zero. A natural window end also turns it off.

```cpp
struct SHitboxPayload { float Damage = 12; };
const auto attack = sequence->WithNotifies({
    {"Hitbox", 0.2f, 0.4f, SHitboxPayload{}} // Starts at 0.2 s, lasts 0.4 s.
});
if (!attack) return;
auto* upperBody = animation->GetLayer("UpperBody");
if (!upperBody) return;
if (!upperBody->GetPlayer().Play(SequenceNode::Create(attack), false)) return;
if (animation->Update(deltaSeconds))
{
    for (const auto& event : animation->GetNotifyEvents())
    {
        if (event.LayerName != "UpperBody" || event.GetName() != "Hitbox") continue;
        const auto* payload = event.GetPayload<SHitboxPayload>();
        if (!payload) continue;
        // Begin activates, Update covers DeltaSeconds, End releases the gameplay state.
        // Cancelled distinguishes interruption from an authored or playback end.
    }
}
```

`SAnimationNotify` owns a name, `Time`, nonnegative `Duration`, and an optional copyable
typed payload. Zero duration emits an `Instant` event. A positive duration defines an
active window whose start is inclusive and end is exclusive. Finite values must satisfy
`0 <= Time` and `Time + Duration <= clip duration`. Windows may overlap or share names; their
immutable list indices identify them independently. `WithNotifies` replaces the notify
list on a cloned sequence, leaving the original asset unchanged.

`SAnimationNotifyEvent` retains its sequence, notify index, layer name, activation
identity, event Type (Instant, Begin, Update, End), frame TimeOffset, active DeltaSeconds,
and Cancelled flag. Each window activation has one Begin and one End. Only Update has a positive
DeltaSeconds, measured in elapsed frame seconds rather than authored clip seconds. A
complete window crossed in one frame still emits all three steps. Begin can occur at the
frame end with no Update yet. At a shared timestamp Updates and Ends precede new Begins
within the layer. Process each successful update batch once; a failed update preserves the
previous batch and all active identities.

Pause and zero delta preserve active windows without emitting Updates. Starting or seeking
inside a window enters it on the next positive update. Playback replacement, seek, montage
replacement, immediate or fading StopMontage, manual pose, Stop, mute, and section jumps
queue cancelled Ends. During an explicit montage exit no action window reopens. Changing
the underlying source while a montage owns the layer keeps its windows. A montage ends
source windows when it starts and returns to source windows at their current phase when it
releases the layer. Removing or clearing layers transfers their Ends to the animation,
retaining metadata after layer destruction.

Queued cancellations are delivered on the next successful Update, including paused or
zero-delta updates; commands do not invoke gameplay callbacks. Continue updating and
consume the batch after commands that stop or remove playback. Animation destruction has
no callback delivery; the gameplay owner releases its resources when destroyed. Queued
cleanup Ends are additional to the existing authored event budget, so stopping an active
set does not require another Begin/Update allocation budget.

Only the target playback emits windows during transitions. Blend spaces use the same
dominant clip and synchronization reference as instantaneous notifies. Marker mapping
preserves active windows across reference cycle boundaries and measures Update overlap in
frame seconds. A dominant-source change ends the previous clip's windows. Muted layers
keep advancing clocks without entering windows. Masks and layer modes affect the pose; a
positive contributing layer can emit its windows.

Montages reuse their validated section intervals. Adjacent sections retain a window that
spans their shared boundary. Nonadjacent links and explicit jumps close it; destination
windows get new activation identities. Skipped windows never emit. A terminal section
closes any remaining active windows at its actual end, even if their authored end lies in
a later section. Source windows may resume within the remaining part of that frame. The
Vulkan pose readback test also exercises lifecycle events during a nonadjacent section
link without changing the compute skinning path.
