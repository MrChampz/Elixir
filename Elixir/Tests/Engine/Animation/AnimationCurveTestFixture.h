#pragma once

#include <gtest/gtest.h>
#include <Engine/Animation/Animation.h>
#include <Engine/Mesh/SkeletalMesh.h>

namespace Elixir::Tests::Curves
{
    /** Supplies a small rig, a scalar ramp, and independent playback for curve tests. */
    class AnimationCurveFixture : public testing::Test
    {
    protected:
        /** @brief Create the reference mesh and one-second scalar ramp. */
        void SetUp() override;

        /**
         * @brief Create a constant scalar clip on the fixture skeleton.
         * @param value Scalar value held over the complete cycle.
         * @param name Curve name.
         */
        Ref<Sequence> Constant(float value, std::string name = "Grip") const;

        Ref<SkeletalMesh> Mesh;
        Ref<Sequence> Ramp;
        Ref<Animation> Playback;
    };
}
