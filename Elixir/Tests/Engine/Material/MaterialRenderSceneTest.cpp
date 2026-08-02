#include <span>
#include <gtest/gtest.h>

#include <Engine/Material/MaterialRenderScene.h>

using namespace Elixir;

TEST(MaterialRenderSceneTest, PreservesAnUnboundMaterialItem)
{
    MaterialRenderScene scene;

    scene.Add({
        .Pass = EMaterialPass::ParticleRibbon,
    });

    const auto items = scene.GetItems();

    ASSERT_EQ(items.size(), 1);
    EXPECT_EQ(items.front().Pass, EMaterialPass::ParticleRibbon);
    EXPECT_FALSE(items.front().Material);
}