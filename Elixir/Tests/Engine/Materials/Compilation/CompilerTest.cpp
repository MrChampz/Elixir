#include <gtest/gtest.h>

#include <Engine/Materials/Compilation/Compiler.h>

using namespace Elixir;
using namespace Elixir::Materials;
using namespace Elixir::Materials::Compilation;

TEST(CompilerTest, AssignsStableSlotsByParameterKindAndName)
{
    MaterialGraph graph;
    const auto material = CreateRef<Material>("Test");
    material->SetGraph(std::move(graph));

    ASSERT_TRUE(material->DefineParameter("Tint", {
        .Kind = EMaterialParameterKind::Value,
        .ValueType = EMaterialValueType::Float4,
        .DefaultValue = SMaterialParameter::MakeVector(glm::vec4(1.0f)),
    }));
    ASSERT_TRUE(material->DefineParameter("Albedo", {
        .Kind = EMaterialParameterKind::Texture,
        .DefaultValue = SMaterialParameter::MakeTexture(nullptr),
    }));

    const auto result = Compiler::Build(*material);

    ASSERT_TRUE(result);
    ASSERT_EQ(result.Material->Parameters.size(), 2);
    EXPECT_EQ(result.Material->Parameters[0].Name, "Albedo");
    EXPECT_EQ(result.Material->Parameters[0].Slot, 0);
    EXPECT_EQ(result.Material->Parameters[1].Name, "Tint");
    EXPECT_EQ(result.Material->Parameters[1].Slot, 0);
}

TEST(CompilerTest, PreservesEnabledRendererUsages)
{
    const auto material = CreateRef<Material>("Material");
    ASSERT_TRUE(material->SetUsage(EMaterialUsage::Surface, true));
    ASSERT_TRUE(material->SetUsage(EMaterialUsage::Particle, true));

    const auto result = Compiler::Build(*material);

    ASSERT_TRUE(result);
    EXPECT_TRUE(result.Material->SupportsUsage(EMaterialUsage::Surface));
    EXPECT_TRUE(result.Material->SupportsUsage(EMaterialUsage::Particle));
}

TEST(CompilerTest, DoesNotAliasParticleUsageShadersToSurfaceShader)
{
    SCompiledMaterial material;

    const auto& surfaceShader = material.GetShader(EMaterialShaderVariant::Surface);
    const auto& spriteShader = material.GetShader(EMaterialShaderVariant::ParticleSprite);
    const auto& ribbonShader = material.GetShader(EMaterialShaderVariant::ParticleRibbon);
    const auto& meshShader = material.GetShader(EMaterialShaderVariant::ParticleMesh);

    EXPECT_EQ(&surfaceShader, &material.SurfaceShader);
    EXPECT_EQ(&spriteShader, &material.ParticleSpriteShader);
    EXPECT_EQ(&ribbonShader, &material.ParticleRibbonShader);
    EXPECT_EQ(&meshShader, &material.ParticleMeshShader);
    EXPECT_FALSE(ribbonShader);
    EXPECT_FALSE(meshShader);
    EXPECT_NE(&ribbonShader, &material.SurfaceShader);
    EXPECT_NE(&meshShader, &material.SurfaceShader);
}
