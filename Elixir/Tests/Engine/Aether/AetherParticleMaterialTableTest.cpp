#include <gtest/gtest.h>

#include <Engine/Aether/ParticleMaterialTable.h>
#include <Engine/Material/MaterialCompiler.h>

using namespace Elixir;
using namespace Elixir::Aether;

TEST(AetherParticleMaterialTableTest, DeduplicatesAProxyAndPreserveItsValues)
{
    auto material = CreateRef<Material>("Particle material");
    ASSERT_TRUE(material->SetUsage(EMaterialUsage::ParticleSprite, true));
    ASSERT_TRUE(material->DefineParameter("Tint", {
        .Kind = EMaterialParameterKind::Value,
        .ValueType = EMaterialGraphValueType::Float4,
        .DefaultValue = SMaterialParam::MakeVector({ 1.0f, 1.0f, 1.0f, 1.0f }),
    }));

    auto instance = CreateRef<MaterialInstance>(material);
    ASSERT_TRUE(instance->SetVector("Tint", { 0.25f, 0.5f, 0.75f, 1.0f }));

    const auto compiled = MaterialCompiler::Build(*material);
    ASSERT_TRUE(compiled);

    const auto proxy = instance->CreateRenderProxy(compiled.Material);
    ASSERT_TRUE(proxy);

    ParticleMaterialTable table{ 1 };
    const auto first = table.Add(*proxy);
    const auto second = table.Add(*proxy);

    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    EXPECT_EQ(*first, 0);
    EXPECT_EQ(*second, 0);
    ASSERT_EQ(table.GetCount(), 1);

    const auto& data = table.GetData()[0];
    EXPECT_EQ(data.Values[0], glm::vec4(0.25f, 0.5f, 0.75f, 1.0f));
    EXPECT_EQ(data.TextureIndices[0], UINT32_MAX);
}

TEST(AetherParticleMaterialTableTest, RejectsAUniqueProxyPastCapacity)
{
    ParticleMaterialTable table{ 0 };

    auto material = CreateRef<Material>("Particle material");
    auto instance = CreateRef<MaterialInstance>(material);
    const auto compiled = MaterialCompiler::Build(*material);
    ASSERT_TRUE(compiled);

    const auto proxy = instance->CreateRenderProxy(compiled.Material);
    ASSERT_TRUE(proxy);
    EXPECT_FALSE(table.Add(*proxy));
}