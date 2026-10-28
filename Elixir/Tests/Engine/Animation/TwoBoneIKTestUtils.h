#pragma once

#include <gtest/gtest.h>
#include <Engine/Animation/Animation.h>
#include <Engine/Animation/TwoBoneIK.h>
#include <Engine/Mesh/SkeletalMesh.h>

namespace Elixir::Tests::IK
{
    /** @brief Create a straight two-segment limb, a tip child, and a root sibling. */
    Ref<SkeletalMesh> MakeIKMesh();

    /**
     * @brief Create independent animation state for a test mesh.
     * @param mesh Mesh supplying the fixed skeleton and reference pose.
     */
    Ref<Animation> MakeIKAnimation(const Ref<const SkeletalMesh>& mesh);

    /**
     * @brief Compare a position with a tolerance suitable for float pose matrices.
     * @param actual Solved position.
     * @param expected Target position.
     */
    void ExpectPosition(const glm::vec3& actual, const glm::vec3& expected);

    /**
     * @brief Compare skeleton-space rotation axes without including inherited scale.
     * @param actual Skeleton-space bone matrix.
     * @param expected Desired unit quaternion.
     */
    void ExpectRotation(const glm::mat4& actual, const glm::quat& expected);

    /**
     * @brief Attach a separately configured solver to an animation's generic pipeline.
     * @param animation Animation that retains the modifier during pose evaluation.
     */
    Ref<TwoBoneIK> AttachIK(Animation& animation);

    /**
     * @brief Configure one limb target on an external solver.
     * @param solver Modifier to configure.
     * @param target Skeleton-space destination.
     * @param pole Skeleton-space bend point.
     * @param weight Rotation blend weight.
     */
    bool SetTarget(
        TwoBoneIK& solver,
        glm::vec3 target,
        glm::vec3 pole = {0, 2, 0},
        float weight = 1
    );

    /** Supplies a reusable limb, independent playback, and an attached solver. */
    class TwoBoneIKFixture : public testing::Test
    {
    protected:
        /**
         * @brief Replace the fixture only after creating valid playback and solver state.
         * @param mesh Mesh supplying the rig and reference pose.
         * @return False if setup fails; retain the previous fixture state.
         */
        bool Initialize(Ref<SkeletalMesh> mesh);

        /** @brief Create the standard limb before each test. */
        void SetUp() override;

        Ref<SkeletalMesh> Mesh;
        Ref<Animation> Playback;
        Ref<TwoBoneIK> Solver;
    };
}
