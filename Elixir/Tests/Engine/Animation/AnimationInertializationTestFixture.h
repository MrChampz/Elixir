#pragma once

#include <gtest/gtest.h>
#include <Engine/Animation/Animation.h>
#include <Engine/Animation/AnimationTransitionMode.h>
#include <Engine/Mesh/SkeletalMesh.h>

namespace Elixir::Tests::Inertia
{
    inline constexpr auto Inertialization = EAnimationTransitionMode::Inertialization;

    /** Supplies a rig, two linear sources, and independent playback for smoothing tests. */
    class AnimationInertializationFixture : public testing::Test
    {
      protected:
        /** @brief Create the rig and moving source clips before each test. */
        void SetUp() override;

        /**
         * @brief Create a linear root translation and scalar curve over five seconds.
         * @param start Initial root translation and curve value.
         * @param speed Units per second on the root X axis and curve.
         */
        Ref<Sequence> Clip(float start, float speed = 0) const;

        /** @brief Create independent playback bound to the fixture rig. */
        Ref<Animation> CreatePlayback() const;

        Ref<SkeletalMesh> Mesh;
        Ref<Sequence> Moving;
        Ref<Sequence> Target;
        Ref<Animation> Playback;
    };
}
