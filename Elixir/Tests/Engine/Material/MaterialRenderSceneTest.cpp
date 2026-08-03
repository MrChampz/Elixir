#include <span>
#include <gtest/gtest.h>

#include <Engine/Material/MaterialRenderScene.h>

using namespace Elixir;

TEST(MaterialRenderSceneTest, PreservesAnUnboundMaterialItem)
{
    MaterialRenderScene scene;
    BufferLayout vertexLayout;
    const auto geometry = scene.AddGeometry({
        .Pipeline = {
            .VertexLayoutKey = 42,
            .VertexLayout = &vertexLayout,
        },
    });

    scene.Add({
        .Pass = EMaterialPass::ParticleRibbon,
        .GeometryIndex = geometry,
    });

    const auto items = scene.GetItems();

    ASSERT_EQ(items.size(), 1);
    EXPECT_EQ(items.front().Pass, EMaterialPass::ParticleRibbon);
    EXPECT_FALSE(items.front().Material);
}

TEST(MaterialRenderSceneTest, ResolvesLateMaterialIndex)
{
    struct SPushConstants
    {
        uint32_t MaterialIndex = UINT32_MAX;
    };

    const auto constants = SMaterialPushConstants::Create(
        SPushConstants{},
        offsetof(SPushConstants, MaterialIndex)
    );

    const auto resolved = constants.Resolve(17);

    SPushConstants values{};
    Memory::Memcpy(&values, resolved.data(), sizeof(values));

    EXPECT_EQ(values.MaterialIndex, 17);
}