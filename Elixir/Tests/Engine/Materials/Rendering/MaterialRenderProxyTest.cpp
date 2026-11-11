#include <gtest/gtest.h>

#include <Engine/Materials/Compilation/Compiler.h>
#include <Engine/Materials/Compilation/MaterialShaderSourceBuilder.h>
#include <Engine/Materials/MaterialInstance.h>
#include <Engine/Materials/Rendering/MaterialRenderProxy.h>

using namespace Elixir;
using namespace Elixir::Materials;
using namespace Elixir::Materials::Compilation;
using namespace Elixir::Materials::Rendering;

TEST(MaterialRenderProxyTest, ResolvesOverridesIntoAnImmutableSnapshot)
{
    const auto material = CreateRef<Material>("Tinted");
    ASSERT_TRUE(material->DefineParameter("Tint", {
        .Kind = EMaterialParameterKind::Value,
        .ValueType = EMaterialValueType::Float4,
        .DefaultValue = SMaterialParameter::MakeVector(glm::vec4(1.0f)),
    }));

    const auto compiled = Compiler::Build(*material).Material;
    ASSERT_TRUE(compiled);

    MaterialInstance instance(material);
    ASSERT_TRUE(instance.SetVector("Tint", { 0.2f, 0.4f, 0.6, 1.0f }));

    const auto proxy = MaterialRenderProxy::Create(compiled, instance);
    ASSERT_TRUE(proxy);
    ASSERT_EQ(proxy->GetValues().size(), 1);
    EXPECT_EQ(proxy->GetValues()[0], glm::vec4(0.2f, 0.4f, 0.6f, 1.0f));
    EXPECT_EQ(proxy->GetInstanceRevision(), instance.GetRevision());
}

TEST(MaterialRenderProxyTest, RejectsACompiledMaterialForAnOldSchema)
{
    const auto material = CreateRef<Material>("Tinted");
    ASSERT_TRUE(material->DefineParameter("Tint", {
        .Kind = EMaterialParameterKind::Value,
        .ValueType = EMaterialValueType::Float4,
        .DefaultValue = SMaterialParameter::MakeVector(glm::vec4(1.0f)),
    }));

    const auto compiled = Compiler::Build(*material).Material;
    ASSERT_TRUE(compiled);
    ASSERT_TRUE(material->SetDefaultParameter(
        "Tint",
        SMaterialParameter::MakeVector(glm::vec4(0.5f))
    ));

    MaterialInstance instance(material);
    EXPECT_FALSE(MaterialRenderProxy::Create(compiled, instance));
}

TEST(MaterialRenderProxyTest, CapturesDoubleSidedSetting)
{
    const auto material = CreateRef<Material>("DoubleSided");
    ASSERT_TRUE(material->SetDoubleSided(true));

    const auto compiled = Compiler::Build(*material).Material;
    ASSERT_TRUE(compiled);

    const MaterialInstance instance(material);
    const auto proxy = MaterialRenderProxy::Create(compiled, instance);

    ASSERT_TRUE(proxy);
    EXPECT_TRUE(proxy->IsDoubleSided());
}


TEST(MaterialRenderProxyTest, CapturesTransmissionWithoutChangingOpaqueCoverage)
{
    const auto material = CreateRef<Material>("Transmission");
    ASSERT_TRUE(material->SetShadingModel(EMaterialShadingModel::Transmission));
    const auto compiled = Compiler::Build(*material).Material;
    ASSERT_TRUE(compiled);
    MaterialInstance instance(material);
    const auto proxy = MaterialRenderProxy::Create(compiled, instance);
    ASSERT_TRUE(proxy);
    EXPECT_EQ(proxy->GetShadingModel(), EMaterialShadingModel::Transmission);
    EXPECT_EQ(proxy->GetBlendMode(), EMaterialBlendMode::Opaque);

    ASSERT_TRUE(material->SetShadingModel(EMaterialShadingModel::Lit));
    EXPECT_EQ(proxy->GetShadingModel(), EMaterialShadingModel::Transmission);
    EXPECT_EQ(MaterialRenderProxy::Create(compiled, instance), nullptr);
}


TEST(MaterialRenderProxyTest, RejectsMissingStorageSlotsBeforeDereferencingThem)
{
    for (const auto kind : {EMaterialParameterKind::Value, EMaterialParameterKind::Texture,
        EMaterialParameterKind::TextureAtlas})
    {
        const auto material = CreateRef<Material>("Missing slot");
        const auto value = kind == EMaterialParameterKind::Value
            ? SMaterialParameter::MakeScalar(1)
            : kind == EMaterialParameterKind::Texture
                ? SMaterialParameter::MakeTexture(nullptr)
                : SMaterialParameter::MakeTextureAtlas(nullptr);
        ASSERT_TRUE(material->DefineParameter(
            "Parameter",
            {.Kind = kind, .ValueType = EMaterialValueType::Float, .DefaultValue = value}
        ));
        const auto compiled = Compiler::Build(*material).Material;
        ASSERT_TRUE(compiled);
        MaterialInstance instance(material);
        const auto original = compiled->Parameters.front();
        for (const bool removeTexture : {true, false})
        {
            auto& parameter = compiled->Parameters.front();
            parameter = original;
            auto& slot = removeTexture ? parameter.TextureSlot : parameter.ValueSlot;
            if (!slot) continue;
            slot.reset();
            EXPECT_FALSE(MaterialRenderProxy::Create(compiled, instance));
            const auto source = MaterialShaderSourceBuilder::Build(
                "",
                *material,
                *compiled,
                EShaderStage::Pixel
            );
            EXPECT_FALSE(source);
            EXPECT_NE(source.Diagnostics.find("missing a storage slot"), std::string::npos);
        }
    }
}

TEST(MaterialRenderProxyTest, RejectsUnknownCompiledKindsEvenWhenBothSlotsExist)
{
    const auto material = CreateRef<Material>("Unknown compiled kind");
    ASSERT_TRUE(material->DefineParameter(
        "Parameter",
        {
            .Kind = EMaterialParameterKind::Value,
            .ValueType = EMaterialValueType::Float,
            .DefaultValue = SMaterialParameter::MakeScalar(1)
        }
    ));
    const auto compiled = Compiler::Build(*material).Material;
    ASSERT_TRUE(compiled);
    auto& parameter = compiled->Parameters.front();
    parameter.Kind = static_cast<EMaterialParameterKind>(255);
    parameter.TextureSlot = 0;
    parameter.ValueSlot = 0;
    MaterialInstance instance(material);
    EXPECT_FALSE(MaterialRenderProxy::Create(compiled, instance));
    const auto source = MaterialShaderSourceBuilder::Build(
        "",
        *material,
        *compiled,
        EShaderStage::Pixel
    );
    EXPECT_FALSE(source);
    EXPECT_NE(
        source.Diagnostics.find("Unsupported material parameter kind"),
        std::string::npos
    );
}
