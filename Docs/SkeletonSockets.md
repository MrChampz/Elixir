# Skeleton sockets

Both characters hold a rigid baton through the `WeaponGrip` socket on `handslot.r`.
The baton is a StaticMesh, rendered separately from the skinned character. It follows
locomotion, Waving, attack montages, additive recoil, and root-motion movement. Pause
with P and step with N to inspect the hand attachment; K starts an attack and B dodges.

`SSkeletonSocket` stores a unique name, a skeleton BoneIndex (not a mesh skin palette
index), and a bone-local RelativeTransform. Pass sockets to Skeleton::Create, or use
WithSockets to clone an imported skeleton with a replacement list. Existing skeletons
and instances remain immutable. Sockets do not change animation compatibility: that
still compares bone names and hierarchy. To use a clone, copy the imported mesh data,
replace its Skeleton, and create the mesh before constructing any instances.

```cpp
auto data = importedMesh->GetData();
const auto hand = data.Skeleton->FindBone("handslot.r");
if (hand < 0) return;
data.Skeleton = data.Skeleton->WithSockets({{
    .Name = "WeaponGrip", .BoneIndex = uint32_t(hand),
    .RelativeTransform = {.Translation = {0, 0.05f, 0}}
}});
if (!data.Skeleton) return;
const auto mesh = SkeletalMesh::Create(std::move(data));
if (!mesh) return;
const auto instance = SkeletalMeshInstance::Create(mesh);
const auto animation = Animation::Create(mesh->GetSkeleton(), mesh->GetData().ReferencePose);
if (!instance || !animation) return;
// After updating animation, on the same thread:
const auto socket = mesh->GetSkeleton()->GetSocketTransform(
    "WeaponGrip",
    animation->GetModelPose()
);
if (socket) renderer.Render(weapon, characterWorld * *socket);
```

Skeleton::GetSocketTransform returns an optional matrix in skeleton space, the same space as
the final model pose before the character's scene transform. The supplied model pose includes
all animation layers and root-motion locking and excludes inverse bind matrices. Multiplication
retains inherited nonuniform scale and shear without decomposing the result. A missing
socket, invalid index, absent pose, or matrix overflow returns an empty optional.
The index overload accepts a cached Skeleton::FindSocket result after checking for -1;
indices belong to that specific skeleton and may differ between socket-list clones.
Queries read the last accepted pose without advancing animation or allocating memory.
Attachments and their scene lifetime remain the caller's responsibility. Notify states
can control when a weapon hitbox is active while a socket supplies its current transform.
