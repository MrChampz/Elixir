# Animation Graph editor

Open **Animation Graph** next to Scene and Strands in the Editor. The pose graph includes
sequence, blend-space, montage action, state-machine, and output nodes. It uses the engine's
existing resources and evaluators; editing never mutates a compiled runtime resource.

## Pose sources and actions

- Use **+ Sequence**, **+ Blend 1D**, **+ Blend 2D**, or **+ Montage** to create resources.
  Select a resource node to edit it. Resources and states can be moved on their canvases.
- Sequence properties include duration, bone-track key times and values, synchronization
  markers, and notify instants or windows. Changing duration rescales the sequence's keys
  and events. The detail timeline displays markers, notifications, and the playback cursor.
- Blend-space properties include sequence samples, their coordinates, and axis parameter
  bindings. The sample plane displays the preview coordinate and actual runtime weights.
  Two-dimensional spaces need at least three distinct, noncollinear points.
- Select a sequence or blend space and choose **Connect to Output Pose** or **Preview this
  source**. The graph compiles the selected source with its numeric parameter bindings.
- Montages are layer actions, not pose-producing nodes. Edit their source sequence, fades,
  interruption policy, section starts, and continuation links. `*` follows timeline order;
  an empty continuation ends playback. **Play Action** in the toolbar plays the selected
  montage. **Play montage over graph**, **Stop montage**, and
  section jump buttons use the layer's real playback controls. The base graph keeps running.
- Resource renaming updates consumers. Deletion requires disconnecting the resource first.
- Click the State Machine node or **Pose / States** to enter the transition graph. Changing
  the view does not change the graph output; use **Use state machine as output** to do so.

## Editing

- Drag a state to move it. Drag the background to pan; scroll to zoom. Reset view restores
  the original camera.
- Use **+ State** to create a state. Select it to edit its name, source, looping policy,
  numeric input bindings, or entry status.
- Select **Connect to...**, then click another state to create a directed transition.
  Click the background to cancel. New transitions are unconditional until conditions are added.
- Click a transition's horizontal wire or label to edit its fade duration, completion policy,
  and conditions. All conditions must match. **Move earlier** increases transition priority.
- Add float or bool parameters and edit their names and defaults in the inspector. Renaming
  updates bindings and transition conditions. Condition comparison buttons cycle through
  `==`, `!=`, `<`, `<=`, `>`, and `>=`; bool conditions require equality or inequality.
- Undo and Redo restore document edits, including positions. History retains 100 edits;
  text fields record each accepted change.

## Runtime preview

**Compile** validates the document and creates a fresh, independent runtime animation.
**Play / Pause** advances that animation. Live inputs appear near the top of the pose-source
inspector. The preview displays sampled time, root translation, montage section, sample
weights, and timeline position. The active state is highlighted in the state-machine view.
Any document edit invalidates the preview; compile again to apply the changes.

The example opens with Locomotion1D connected to output. Live Speed blends Idle at 0, Walk
at 1.5, and Run at 4. Directional2D demonstrates a triangular sample plane. The Action montage
has Start, Action, and End sections; Action repeats until stopped or jumped to End.
The sample sequences animate one root bone, allowing
pose changes to be inspected numerically; the preview does not render a character mesh.

For state-machine playback, select **Use state machine as output**, then Compile and Play.
Speed above 0.1 enters Walk; above 3 enters Run; 0 returns to Idle.

## Persistence and source integration

Enter a path in the toolbar and use **Save** or **Load**. Relative paths use the Editor's
working directory. `.animgraph` files store names, typed defaults, states, ordered
transitions, conditions, input bindings, and canvas positions. Version 2 also stores complete
skeletons, sequence tracks and events, blend samples, montage sections, and the output
connection. The editor still reads version 1 documents. Files are versioned and
validated before replacing the open document. Save current edits before loading a file.
Saving writes a temporary file before replacing the destination.

`AnimationGraphPanel(document, sources)` opens application-provided state-machine definitions.
The source map associates stable names with immutable `AnimationNode` instances. These can
include sequence, blend-space, blend, or nested state-machine nodes. External sources still
need the same source names registered when loading. Resources authored in the document's
`Library` are self-contained and load without an external source registry. Arbitrary notify
payload objects cannot be serialized; saving rejects them rather than dropping their data.
There is no project animation asset browser or mesh importer in this workspace.

## Validation

Build `Editor` and `Editor.AnimationTests`, then run:

```sh
ctest --test-dir <build-directory> -R 'AnimationGraph(Document|Resources)Test' --output-on-failure
```

Tests cover reference updates after renaming/deletion, file round trips, failed-load
preservation, typed binding validation, independent transitions, self-contained resource
persistence, real 1D/2D blended poses, edited state sources, and montage playback and return.
