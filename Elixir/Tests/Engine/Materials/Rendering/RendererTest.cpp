#include <gtest/gtest.h>

#include <Engine/Materials/Compilation/Compiler.h>
#include <Engine/Materials/Rendering/MaterialRenderScene.h>
#include <Engine/Materials/Rendering/Renderer.h>

using namespace Elixir;
using namespace Elixir::Materials;
using namespace Elixir::Materials::Compilation;
using namespace Elixir::Materials::Rendering;

namespace
{
    template <typename T>
    concept HasMaterialIndex = requires(T value)
    {
        value.MaterialIndex;
    };

    static_assert(!HasMaterialIndex<SResolvedRenderItem>);

    Ref<MaterialInstance> CreateInstance()
    {
        const auto material = CreateRef<Material>("Renderer test");
        EXPECT_TRUE(material->DefineParameter("Tint", {
            .Kind = EMaterialParameterKind::Value,
            .ValueType = EMaterialValueType::Float4,
            .DefaultValue = SMaterialParameter::MakeVector(glm::vec4(1.0f)),
        }));

        return material->CreateInstance();
    }
}

TEST(RendererTest, MapsParticlePassesToMaterialUsages)
{
    EXPECT_EQ(
        Renderer::GetUsage(EMaterialPass::ParticleSprite),
        EMaterialUsage::ParticleSprite
    );
    EXPECT_EQ(
        Renderer::GetUsage(EMaterialPass::ParticleRibbon),
        EMaterialUsage::ParticleRibbon
    );
    EXPECT_EQ(
        Renderer::GetUsage(EMaterialPass::ParticleMesh),
        EMaterialUsage::ParticleMesh
    );
}

TEST(RendererTest, PreparedSceneKeepsResolvedMaterialDataWithTheSourceItem)
{
    const auto instance = CreateInstance();
    const auto compiled = Compiler::Build(*instance->GetParent()).Material;
    const auto proxy = MaterialRenderProxy::Create(compiled, *instance);

    ASSERT_TRUE(proxy);

    MaterialRenderScene scene;
    BufferLayout vertexLayout;
    const auto geometry = scene.AddGeometry({
        .Pipeline = {
            .VertexLayoutKey = 42,
            .VertexLayout = &vertexLayout,
        },
    });
    scene.Add({
        .Material = instance,
        .GeometryIndex = geometry,
    });

    const auto items = scene.GetItems();
    const SPreparedScene prepared{
        .Scene = &scene,
        .Items = {
            {
                .Item = &items.front(),
                .Proxy = proxy,
            },
        },
    };

    ASSERT_EQ(prepared.Items.size(), 1);
    EXPECT_EQ(prepared.Scene, &scene);
    EXPECT_EQ(prepared.Items.front().Item, &items.front());
    EXPECT_EQ(prepared.Items.front().Proxy, proxy);
}
