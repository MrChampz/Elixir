#include "AnimationCurveTestFixture.h"
#include "../Mesh/SkinningTestData.h"

namespace Elixir::Tests::Curves
{
    void AnimationCurveFixture::SetUp()
    {
        Mesh = SkinningTests::MakeMesh();
        ASSERT_TRUE(Mesh);
        Ramp = Sequence::Create(
            "Ramp",
            Mesh->GetSkeleton(),
            1,
            {},
            {},
            {},
            {{"Grip", {{0, 0}, {1, 1}}}}
        );
        ASSERT_TRUE(Ramp);
        Playback = Animation::Create(Mesh->GetSkeleton(), Mesh->GetData().ReferencePose);
        ASSERT_TRUE(Playback);
    }

    Ref<Sequence> AnimationCurveFixture::Constant(float value, std::string name) const
    {
        return Ramp->WithCurves({{std::move(name), {{0, value}}}});
    }
}
