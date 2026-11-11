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
    EXPECT_EQ(result.Material->Parameters[0].TextureSlot, 0u);
    EXPECT_FALSE(result.Material->Parameters[0].ValueSlot);
    EXPECT_EQ(result.Material->Parameters[1].Name, "Tint");
    EXPECT_EQ(result.Material->Parameters[1].ValueSlot, 0u);
    EXPECT_FALSE(result.Material->Parameters[1].TextureSlot);
}

TEST(CompilerTest, CompilesOnlyTheSelectedRendererUsage)
{
    const auto material = CreateRef<Material>("Material", EMaterialUsage::Particle);

    const auto result = Compiler::Build(*material);

    ASSERT_TRUE(result);
    EXPECT_EQ(result.Material->GetUsage(), EMaterialUsage::Particle);
}

TEST(CompilerTest, DoesNotAliasParticleUsageShadersToSurfaceShader)
{
    SCompiledMaterial material;

    const auto& surfaceShader = material.GetShader(EMaterialShaderVariant::Surface);
    const auto& spriteShader = material.GetShader(EMaterialShaderVariant::ParticleSprite);
    const auto& ribbonShader = material.GetShader(EMaterialShaderVariant::ParticleRibbon);
    const auto& meshShader = material.GetShader(EMaterialShaderVariant::ParticleMesh);
    const auto& strandShader = material.GetShader(EMaterialShaderVariant::ParticleStrand);

    EXPECT_EQ(&surfaceShader, &material.SurfaceShader);
    EXPECT_EQ(&spriteShader, &material.ParticleSpriteShader);
    EXPECT_EQ(&ribbonShader, &material.ParticleRibbonShader);
    EXPECT_EQ(&meshShader, &material.ParticleMeshShader);
    EXPECT_EQ(&strandShader, &material.ParticleStrandShader);
    EXPECT_FALSE(ribbonShader);
    EXPECT_FALSE(meshShader);
    EXPECT_FALSE(strandShader);
    EXPECT_NE(&ribbonShader, &material.SurfaceShader);
    EXPECT_NE(&meshShader, &material.SurfaceShader);
    EXPECT_NE(&strandShader, &material.SurfaceShader);
}

TEST(CompilerTest, AssignsIndependentOptionalSlotsForMixedValuesTexturesAndAtlases)
{
    const auto material = CreateRef<Material>("Mixed slots");
    const std::array kinds{
        EMaterialParameterKind::Value,
        EMaterialParameterKind::Texture,
        EMaterialParameterKind::TextureAtlas,
        EMaterialParameterKind::Value,
        EMaterialParameterKind::Texture,
        EMaterialParameterKind::TextureAtlas
    };
    for (size_t index = 0; index < kinds.size(); ++index)
    {
        const auto kind = kinds[index];
        const auto value = kind == EMaterialParameterKind::Value
            ? SMaterialParameter::MakeScalar(1)
            : kind == EMaterialParameterKind::Texture
                ? SMaterialParameter::MakeTexture(nullptr)
                : SMaterialParameter::MakeTextureAtlas(nullptr);
        ASSERT_TRUE(material->DefineParameter(
            std::string(1, static_cast<char>('A' + index)),
            {.Kind = kind, .ValueType = EMaterialValueType::Float, .DefaultValue = value}
        ));
    }
    const auto result = Compiler::Build(*material);
    ASSERT_TRUE(result) << result.Diagnostics;
    ASSERT_EQ(result.Material->Parameters.size(), kinds.size());
    const std::array<std::optional<uint32_t>, 6> textureSlots{std::nullopt, 0, 1,
        std::nullopt, 2, 3};
    const std::array<std::optional<uint32_t>, 6> valueSlots{0, std::nullopt, 1,
        2, std::nullopt, 3};
    for (size_t index = 0; index < kinds.size(); ++index)
    {
        const auto& parameter = result.Material->Parameters[index];
        EXPECT_EQ(parameter.TextureSlot, textureSlots[index]) << parameter.Name;
        EXPECT_EQ(parameter.ValueSlot, valueSlots[index]) << parameter.Name;
    }
}

TEST(CompilerTest, RejectsUnknownParameterKindsInsteadOfTreatingThemAsValues)
{
    Material material("Unknown parameter kind");
    EXPECT_FALSE(material.DefineParameter(
        "Unknown",
        {
            .Kind = static_cast<EMaterialParameterKind>(255),
            .ValueType = EMaterialValueType::Float4,
            .DefaultValue = SMaterialParameter::MakeVector(glm::vec4(1))
        }
    ));
    EXPECT_TRUE(material.GetParameters().empty());
}

TEST(CompilerTest, ParticleLightingCompilesLitAndUnlitAndRejectsSurfaceOnlyModels)
{
    Material particle("Particle lighting", EMaterialUsage::Particle);
    EXPECT_TRUE(Compiler::Build(particle));
    ASSERT_TRUE(particle.SetShadingModel(EMaterialShadingModel::Unlit));
    EXPECT_TRUE(Compiler::Build(particle));
    for (const auto model : {
        EMaterialShadingModel::ClearCoat,
        EMaterialShadingModel::Subsurface,
        EMaterialShadingModel::Cloth,
        EMaterialShadingModel::Transmission})
    {
        EXPECT_FALSE(particle.SetShadingModel(model));
        EXPECT_EQ(particle.GetShadingModel(), EMaterialShadingModel::Unlit);
        EXPECT_TRUE(Compiler::Build(particle));
        Material surface("Surface lighting", EMaterialUsage::Surface);
        surface.SetShadingModel(model);
        EXPECT_TRUE(Compiler::Build(surface));
    }
}
