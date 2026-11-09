#include <gtest/gtest.h>
#include <Engine/Aether/Effect/Effect.h>
#include <Engine/Aether/Effect/MaterialDescription.h>
#include <Engine/Graphics/Pipeline/PipelineBuilder.h>
#include <Engine/Materials/TextureAtlas.h>
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
        "material":{"texture":"base.png","blendMode":"Additive",
            "atlas":{"columns":8,"rows":4,"frames":30,"interpolate":true},
            "emissiveTexture":"emission.png","emissiveScale":3,
            "colorRampTexture":"ramp.png","normalTexture":"normal.png",
            "normalLightDirection":[0,1,0],"ambientLight":0.2,
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
    EXPECT_EQ(material.Atlas.Frames, 30u);
    EXPECT_TRUE(material.Atlas.Interpolate);
    EXPECT_EQ(material.BlendMode, Materials::EMaterialBlendMode::Additive);
    EXPECT_EQ(material.EmissiveTexturePath, "emission.png");
    EXPECT_EQ(material.ColorRampTexturePath, "ramp.png");
    EXPECT_EQ(material.NormalTexturePath, "normal.png");
    EXPECT_EQ(material.RefractionTexturePath, "distortion.png");
    EXPECT_FLOAT_EQ(material.EmissiveScale, 3);
    EXPECT_FLOAT_EQ(material.RefractionStrength, 0.03f);
    auto changed = material;
    changed.Atlas.Interpolate = false;
    EXPECT_NE(material, changed);
}

TEST(ParticleMaterialsTest, RejectsInvalidLayoutsAndTiming)
{
    for (const auto* settings :
         {R"("atlas":{"columns":0,"rows":1,"frames":1})",
          R"("atlas":{"columns":2,"rows":2,"frames":5})",
          R"("atlas":{"columns":4294967295,"rows":4294967295,"frames":1})",
          R"("atlas":{"columns":1,"rows":1,"frames":1,"interpolate":3})",
          R"("emissiveScale":-1)",
          R"("ambientLight":2)",
          R"("normalLightDirection":[0,0,0])",
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
