#include <gtest/gtest.h>

#include <Engine/Material/MaterialCompiler.h>

using namespace Elixir;

TEST(MaterialCompilerTest, AssignsStableSlotsByParameterKindAndName)
{
    MaterialGraph graph;
    const auto material = CreateRef<Material>("Test");
    material->SetGraph(std::move(graph));

    ASSERT_TRUE(material->DefineParameter("Tint", {
        .Kind = EMaterialParameterKind::Value,
        .ValueType = EMaterialValueType::Float4,
        .DefaultValue = SMaterialParam::MakeVector(glm::vec4(1.0f)),
    }));
    ASSERT_TRUE(material->DefineParameter("Albedo", {
        .Kind = EMaterialParameterKind::Texture,
        .DefaultValue = SMaterialParam::MakeTexture(nullptr),
    }));

    const auto result = MaterialCompiler::Build(*material);

    ASSERT_TRUE(result);
    ASSERT_EQ(result.Material->Parameters.size(), 2);
    EXPECT_EQ(result.Material->Parameters[0].Name, "Albedo");
    EXPECT_EQ(result.Material->Parameters[0].Slot, 0);
    EXPECT_EQ(result.Material->Parameters[1].Name, "Tint");
    EXPECT_EQ(result.Material->Parameters[1].Slot, 0);
}

TEST(MaterialCompilerTest, PreservesEnabledRendererUsages)
{
    const auto material = CreateRef<Material>("Particle material");
    ASSERT_TRUE(material->SetUsage(EMaterialUsage::ParticleSprite, true));
    ASSERT_TRUE(material->SetUsage(EMaterialUsage::ParticleRibbon, true));
    ASSERT_TRUE(material->SetUsage(EMaterialUsage::ParticleMesh, true));

    const auto result = MaterialCompiler::Build(*material);

    ASSERT_TRUE(result);
    EXPECT_TRUE(result.Material->SupportsUsage(EMaterialUsage::ParticleSprite));
    EXPECT_TRUE(result.Material->SupportsUsage(EMaterialUsage::ParticleRibbon));
    EXPECT_TRUE(result.Material->SupportsUsage(EMaterialUsage::ParticleMesh));
}

TEST(MaterialCompilerTest, DoesNotAliasParticleUsageShadersToSurfaceShader)
{
    SCompiledMaterial material;

    const auto& spriteShader = material.GetShader(EMaterialUsage::ParticleSprite);
    const auto& ribbonShader = material.GetShader(EMaterialUsage::ParticleRibbon);
    const auto& meshShader = material.GetShader(EMaterialUsage::ParticleMesh);

    EXPECT_EQ(&spriteShader, &material.ParticleSpriteShader);
    EXPECT_EQ(&ribbonShader, &material.ParticleRibbonShader);
    EXPECT_EQ(&meshShader, &material.ParticleMeshShader);
    EXPECT_FALSE(ribbonShader);
    EXPECT_FALSE(meshShader);
    EXPECT_NE(&ribbonShader, &material.SurfaceShader);
    EXPECT_NE(&meshShader, &material.SurfaceShader);
}