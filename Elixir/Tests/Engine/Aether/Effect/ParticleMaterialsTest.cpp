#include <gtest/gtest.h>
#include <Engine/Aether/Rendering/Renderer.h>
#include <cstddef>
#include <Engine/Aether/Effect/Effect.h>
#include <Engine/Aether/Effect/MaterialDescription.h>
#include <Engine/Aether/Effect/MaterialFactory.h>
#include <Engine/Graphics/Pipeline/PipelineBuilder.h>
#include <Engine/Graphics/TextureAtlas.h>
#include <limits>

using namespace Elixir;
using namespace Elixir::Aether;

TEST(ParticleMaterialsTest, ParsesAtlasAndGenericTextureProperties)
{
    std::string error;
    const auto system = Effect::ParseEffect(
        R"({"name":"Materials","emitters":[{
        "name":"Burst","maxParticles":16,"spawnRate":0,
        "burst":{"count":4,"interval":0,"delay":0.5},
        "material":{"texture":"base.png","blendMode":"Additive","shadingModel":"Unlit",
            "atlas":{"columns":8,"rows":4,"interpolate":true},
            "emissiveTexture":"emission.png","emissiveScale":3,
            "colorRampTexture":"ramp.png","normalTexture":"normal.png",
            "refractionTexture":"distortion.png","refractionStrength":0.03}}]})",
        &error
    );
    ASSERT_TRUE(system) << error;
    const auto& emitter = *system->GetEmitters().front();
    EXPECT_FLOAT_EQ(emitter.GetBurstDelay(), 0.5f);
    ASSERT_TRUE(emitter.GetMaterialDescription());
    const auto& material = *emitter.GetMaterialDescription();
    EXPECT_EQ(material.Atlas.Columns, 8u);
    EXPECT_EQ(material.Atlas.Rows, 4u);
    EXPECT_TRUE(material.InterpolateAtlas);
    EXPECT_EQ(material.BlendMode, Materials::EMaterialBlendMode::Additive);
    EXPECT_EQ(material.ShadingModel, Materials::EMaterialShadingModel::Unlit);
    EXPECT_EQ(material.EmissiveTexturePath, "emission.png");
    EXPECT_EQ(material.ColorRampTexturePath, "ramp.png");
    EXPECT_EQ(material.NormalTexturePath, "normal.png");
    EXPECT_EQ(material.RefractionTexturePath, "distortion.png");
    EXPECT_FLOAT_EQ(material.EmissiveScale, 3);
    EXPECT_FLOAT_EQ(material.RefractionStrength, 0.03f);
    auto changed = material;
    changed.InterpolateAtlas = false;
    EXPECT_NE(material, changed);
}

TEST(ParticleMaterialsTest, RejectsInvalidLayoutsAndTiming)
{
    for (const auto* settings :
         {R"("atlas":{"columns":0,"rows":1})",
          R"("atlas":{"columns":2,"rows":0})",
          R"("atlas":{"columns":1,"rows":1,"interpolate":3})",
          R"("emissiveScale":-1)",
          R"("shadingModel":"Unknown")",
          R"("shadingModel":"Transmission")",
          R"("refractionStrength":-1)"})
    {
        std::string error;
        const std::string json = R"({"name":"Invalid","emitters":[{"name":"E",
            "maxParticles":16,"spawnRate":0,"material":{)" +
                                 std::string(settings) + "}}]}";
        EXPECT_FALSE(Effect::ParseEffect(json, &error)) << settings;
        EXPECT_FALSE(error.empty());
    }
    Emitter emitter("Delay", 16, 0);
    EXPECT_FALSE(emitter.SetBurstDelay(-1));
    EXPECT_FALSE(emitter.SetBurstDelay(std::numeric_limits<float>::infinity()));
    EXPECT_FALSE(emitter.SetBurstDelay(std::numeric_limits<float>::quiet_NaN()));
    EXPECT_FLOAT_EQ(emitter.GetBurstDelay(), 0);
    std::string error;
    EXPECT_FALSE(
        Effect::ParseEffect(
            R"({"name":"Invalid","emitters":[{"name":"E",
        "maxParticles":16,"spawnRate":0,"burst":{"count":4,"interval":0,"delay":-1}}]})",
            &error
        )
    );
}

TEST(ParticleMaterialsTest, MaterialDescriptionAcceptsValidTextureSettings)
{
    Effect::SMaterialDescription description;
    EXPECT_TRUE(description.IsValid());
    description.Atlas = {8, 4};
    description.EmissiveScale = 0;
    description.RefractionStrength = 0;
    EXPECT_TRUE(description.IsValid());
    description.EmissiveScale = 3;
    description.RefractionStrength = 0.03f;
    EXPECT_TRUE(description.IsValid());
}

TEST(ParticleMaterialsTest, MaterialDescriptionAndFactoryRejectInvalidTextureSettings)
{
    for (const auto layout : {STextureAtlasLayout{0, 1}, STextureAtlasLayout{1, 0}})
    {
        Effect::SMaterialDescription description;
        description.Atlas = layout;
        EXPECT_FALSE(description.IsValid());
        EXPECT_FALSE(Effect::CreateMaterial("Invalid atlas", description));
    }
    for (const float invalid : {-1.0f, std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        Effect::SMaterialDescription description;
        description.EmissiveScale = invalid;
        EXPECT_FALSE(description.IsValid());
        EXPECT_FALSE(Effect::CreateMaterial("Invalid emissive scale", description));
        description.EmissiveScale = 1;
        description.RefractionStrength = invalid;
        EXPECT_FALSE(description.IsValid());
        EXPECT_FALSE(Effect::CreateMaterial("Invalid refraction strength", description));
    }
}

TEST(ParticleMaterialsTest, FactoryPreservesSupportedShadingModelsAndRejectsOthers)
{
    using Materials::EMaterialShadingModel;
    for (const auto model : {EMaterialShadingModel::Lit, EMaterialShadingModel::Unlit})
    {
        Effect::SMaterialDescription description;
        description.ShadingModel = model;
        EXPECT_TRUE(description.IsValid());
        const auto material = Effect::CreateMaterial("Supported shading", description);
        ASSERT_TRUE(material);
        EXPECT_EQ(material->GetShadingModel(), model);
    }
    for (const auto model : {EMaterialShadingModel::ClearCoat, EMaterialShadingModel::Subsurface,
        EMaterialShadingModel::Cloth, EMaterialShadingModel::Transmission,
        static_cast<EMaterialShadingModel>(255)})
    {
        Effect::SMaterialDescription description;
        description.ShadingModel = model;
        EXPECT_FALSE(description.IsValid());
        EXPECT_FALSE(Effect::CreateMaterial("Unsupported shading", description));
    }
}

TEST(ParticleMaterialsTest, AdditiveBlendPreservesBackgroundAndAlpha)
{
    PipelineBuilder builder;
    builder.EnableAdditiveBlending();
    const auto info = builder.GetCreateInfo();
    EXPECT_TRUE(info.ColorBlend.Attachments.front().BlendEnable);
    EXPECT_EQ(info.ColorBlend.Attachments.front().SrcColorBlendFactor, EBlendFactor::SrcAlpha);
    EXPECT_EQ(info.ColorBlend.Attachments.front().DstColorBlendFactor, EBlendFactor::One);
    EXPECT_EQ(info.ColorBlend.Attachments.front().SrcAlphaBlendFactor, EBlendFactor::Zero);
    EXPECT_EQ(info.ColorBlend.Attachments.front().DstAlphaBlendFactor, EBlendFactor::One);
}


TEST(ParticleMaterialsTest, FrameLightDataMatchesShaderPacking)
{
    using Elixir::Aether::Rendering::SFrameData;
    EXPECT_EQ(offsetof(SFrameData, LightDirection), 208u);
    EXPECT_EQ(offsetof(SFrameData, LightColor), 224u);
    EXPECT_EQ(sizeof(SFrameData), 240u);
}
