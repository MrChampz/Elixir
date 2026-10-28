#include "TwoBoneIKTestUtils.h"
#include "../Mesh/SkinningTestData.h"

namespace Elixir::Tests::IK
{
    /** @brief Create a straight two-segment limb, a tip child, and a root sibling. */
    Ref<SkeletalMesh> MakeIKMesh()
    {
        auto data = SkinningTests::MakeMesh()->GetData();
        data.Skeleton = Skeleton::Create(
            "Limb",
            {
                {.Name = "Root"},
                {.Name = "Middle", .ParentIndex = 0,
                    .ReferenceTransform = {.Translation = {1, 0, 0}}},
                {.Name = "End", .ParentIndex = 1,
                    .ReferenceTransform = {.Translation = {1, 0, 0}}},
                {.Name = "Finger", .ParentIndex = 2,
                    .ReferenceTransform = {.Translation = {0.2f, 0, 0}}},
                {.Name = "Sibling", .ParentIndex = 0,
                    .ReferenceTransform = {.Translation = {0, 1, 0}}}
            },
            {{.Name = "Grip", .BoneIndex = 2}}
        );
        data.ReferencePose.LocalTransforms.clear();
        return SkeletalMesh::Create(std::move(data));
    }

    /**
     * @brief Create independent animation state for a test mesh.
     * @param mesh Mesh supplying the fixed skeleton and reference pose.
     */
    Ref<Animation> MakeIKAnimation(const Ref<const SkeletalMesh>& mesh)
    {
        if (!mesh) return nullptr;
        return Animation::Create(mesh->GetSkeleton(), mesh->GetData().ReferencePose);
    }

    /**
     * @brief Compare a position with a tolerance suitable for float pose matrices.
     * @param actual Solved position.
     * @param expected Target position.
     */
    void ExpectPosition(const glm::vec3& actual, const glm::vec3& expected)
    {
        EXPECT_NEAR(actual.x, expected.x, 1e-4f);
        EXPECT_NEAR(actual.y, expected.y, 1e-4f);
        EXPECT_NEAR(actual.z, expected.z, 1e-4f);
    }

    /**
     * @brief Attach a separately configured solver to an animation's generic pipeline.
     * @param animation Animation that retains the modifier during pose evaluation.
     */
    Ref<TwoBoneIK> AttachIK(Animation& animation)
    {
        const auto solver = TwoBoneIK::Create(animation.GetSkeleton());
        if (!solver) return nullptr;
        const std::array<Ref<const PoseModifier>, 1> modifiers{solver};
        return animation.SetPoseModifiers(modifiers) ? solver : nullptr;
    }

    /**
     * @brief Configure one limb target on an external solver.
     * @param solver Modifier to configure.
     * @param target Skeleton-space destination.
     * @param pole Skeleton-space bend point.
     * @param weight Rotation blend weight.
     */
    bool SetTarget(TwoBoneIK& solver, glm::vec3 target, glm::vec3 pole, float weight)
    {
        const STwoBoneIKConstraint constraint{2, target, pole, weight};
        return solver.SetConstraints(std::span(&constraint, 1));
    }
    /**
     * @brief Compare skeleton-space rotation axes without including inherited scale.
     * @param actual Skeleton-space bone matrix.
     * @param expected Desired unit quaternion.
     */
    void ExpectRotation(const glm::mat4& actual, const glm::quat& expected)
    {
        const auto rotation = glm::mat3_cast(expected);
        for (int axis = 0; axis < 3; ++axis)
            ExpectPosition(glm::normalize(glm::vec3(actual[axis])), rotation[axis]);
    }
    bool TwoBoneIKFixture::Initialize(Ref<SkeletalMesh> mesh)
    {
        if (!mesh) return false;
        auto playback = MakeIKAnimation(mesh);
        if (!playback) return false;
        auto solver = AttachIK(*playback);
        if (!solver) return false;
        Mesh = std::move(mesh);
        Playback = std::move(playback);
        Solver = std::move(solver);
        return true;
    }

    void TwoBoneIKFixture::SetUp()
    {
        ASSERT_TRUE(Initialize(MakeIKMesh()));
    }
}
