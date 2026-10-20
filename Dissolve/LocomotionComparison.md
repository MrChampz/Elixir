# KayKit synchronization comparison

Dissolve displays the same model, blend coordinates, cycle duration, and reference phase
on both sides. The left instance samples normalized time. The right instance maps time
between named synchronization markers. Particle drawing is disabled in this demo so the
legs remain visible. A small bearing correction keeps both models at the same viewing angle.
Meshes and original animation tracks are unchanged.

## Authored support markers

The following times are in seconds, for the six-clip `KayKitCharacter/Locomotion.glb`.
The markers identify **full support**, rather than the initial heel or toe strike.

| Clip | Duration | LeftFootPlant | RightFootPlant |
| --- | ---: | ---: | ---: |
| Walking_A | 1.066667 | 0.321778 | 0.862222 |
| Running_A | 0.800000 | 0.114667 | 0.514667 |
| Walking_Backwards | 1.066667 | 0.816000 | 0.282667 |
| Running_Strafe_Left | 0.800000 | 0.149333 | 0.561333 |
| Running_Strafe_Right | 0.800000 | 0.157333 | 0.545333 |
| Idle_A | 1.066667 | none | none |

The annotations are applied with `WithSyncMarkers` in `InitializeAnimationController`.
Idle keeps normalized sampling because both feet remain supported throughout its cycle.

### Measurement and checks

The GLB's original translation, scale, and quaternion channels were sampled at 600 equally
spaced points per cycle. World transforms of `foot.l`, `foot.r`, `toes.l`, and `toes.r`
were reconstructed through the hierarchy. The lowest ankle positions during each stance
were selected as reproducible full-support markers. Sole geometry was also skinned to
check that the chosen points occur during support, rather than during the airborne swing.
Sampling resolution is approximately 1.3 ms for running and 1.8 ms for walking. These
annotations target this particular stylized model; they are not an automatic contact
detection feature of the engine.

The backward clip is substantially out of phase with the strafes. Its left support is at
76.5% of the cycle, compared with 18.7% for the left strafe. Their circular phase mismatch
is approximately **42% of a cycle**. The walk/run pair differs by approximately **16%**.

The fixed synchronization reference is the first marked sample in sorted coordinate order:
`Running_Strafe_Left`. Its left marker defines reference phase 0.186667. At that phase,
each marked clip samples its own left support time, even when the reference has zero weight.
Changing blend coordinates therefore retains the shared contact phase.

## Reproduce the comparison

- `1`: blend Walking_A and Running_A equally, at (0, 2.25).
- `2`: blend forward locomotion and the right strafe, at (1, 1).
- `3`: blend Walking_Backwards and the left strafe equally, at (-1.5, -0.75).
  This is the initial preset and makes the phase mismatch easiest to see.
- `WASD`: temporary movement direction; `Shift` or `R` increases speed.
- Arrow keys: retain and adjust either blend coordinate after releasing the key.
- `Space`: return the retained coordinate to idle.
- `P`: pause or resume both instances.
- `N`: pause and advance both instances by one 1/30-second step.
- `F`: pause both instances at the reference's left support marker. Set a preset and
  let the coordinates settle before pressing F.

The synchronized example should show the same supporting foot across the contributing
clips. The normalized example may blend opposing support and swing phases, particularly
with preset 3. Foot geometry, body lean, and stride length still differ between the source
clips. Synchronization does not provide foot locking, ground collision, root motion, or IK.
