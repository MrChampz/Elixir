#include "AnimationInertializationTestFixture.h"
#include "../Mesh/SkinningTestData.h"

namespace Elixir::Tests::Inertia
{
    void AnimationInertializationFixture::SetUp()
    {
        Mesh = SkinningTests::MakeMesh();
        ASSERT_TRUE(Mesh);
        Moving = Clip(0, 2);
        Target = Clip(10, -3);
        ASSERT_TRUE(Moving);
        ASSERT_TRUE(Target);
        Playback = CreatePlayback();
        ASSERT_TRUE(Playback);
    }

    Ref<Sequence> AnimationInertializationFixture::Clip(float start, float speed) const
    {
        return Sequence::Create(
            "Clip",
            Mesh->GetSkeleton(),
            5,
            {{.BoneIndex = 0,
              .Translations = {{0, {start, 0, 0}}, {5, {start + 5 * speed, 0, 0}}}}},
            {},
            {},
            {{"Value", {{0, start}, {5, start + 5 * speed}}}}
        );
    }

    Ref<Animation> AnimationInertializationFixture::CreatePlayback() const
    {
        return Animation::Create(Mesh->GetSkeleton(), Mesh->GetData().ReferencePose);
    }
}
