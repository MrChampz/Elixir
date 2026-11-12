#include "MaterialGraphTestEmission.h"
#include <gtest/gtest.h>
#include <Engine/Materials/Nodes/TextureAtlasSample.h>
#include <Engine/Materials/Nodes/Constant.h>
#include <Engine/Materials/Nodes/Multiply.h>
#include <Engine/Materials/Nodes/ParticleTime.h>
#include <Engine/Materials/Nodes/SceneColor.h>

#include <Engine/Graphics/TextureAtlas.h>
#include <Engine/Materials/Compilation/Compiler.h>
#include <Engine/Materials/MaterialInstance.h>
using namespace Elixir::Materials;
using namespace Elixir::Materials::Nodes;
using namespace Elixir::Materials::Compilation;
using namespace Elixir::Materials::Tests;

TEST(MaterialAtlasTest, AtlasSamplingAcceptsFramesFromAnyGraph)
{
    MaterialGraph graph;
    const auto atlas = graph.AddNode<TextureAtlasSample>("Atlas", true);
    graph.SetChannel(EMaterialChannel::BaseColor, atlas);
    SMaterialGraphBindings bindings;
    bindings.Textures["Atlas"] = "3u";
    bindings.AtlasLayouts["Atlas"] = "AtlasLayout";
    for (const auto usage : {EMaterialUsage::Surface, EMaterialUsage::Particle})
    {
        const auto source = EmitGraph(graph, bindings, EShaderStage::Pixel, usage);
        EXPECT_NE(source.find("SampleTextureAtlas("), std::string::npos);
        EXPECT_NE(source.find("uint2(AtlasLayout.xy)"), std::string::npos);
        EXPECT_NE(source.find("true"), std::string::npos);
    }
    EXPECT_EQ(SupportedStages(graph, atlas), EShaderStage::Pixel);
}

TEST(MaterialAtlasTest, SceneSamplingUsesScreenCoordinatesAndIsPixelOnly)
{
    MaterialGraph graph;
    const auto scene = graph.AddNode<SceneColor>();
    graph.SetChannel(EMaterialChannel::Emissive, scene);
    for (const auto usage : {EMaterialUsage::Surface, EMaterialUsage::Particle})
    {
        const auto source = EmitGraph(graph, {}, EShaderStage::Pixel, usage);
        EXPECT_NE(source.find("SampleMaterialSceneColor("), std::string::npos);
        EXPECT_NE(source.find("input.ClipPos.xy"), std::string::npos);
    }
    EXPECT_EQ(SupportedStages(graph, scene), EShaderStage::Pixel);
}

TEST(MaterialAtlasTest, ParticleTimingUsesRendererInputs)
{
    MaterialGraph graph;
    const auto time = graph.AddNode<ParticleTime>();
    graph.SetChannel(EMaterialChannel::Opacity, time);
    const auto source = EmitGraph(graph, {}, EShaderStage::Pixel, EMaterialUsage::Particle);
    EXPECT_NE(source.find("input.ParticleTime.x"), std::string::npos);
    EXPECT_NE(source.find("input.ParticleTime.y"), std::string::npos);
    const auto environment = MaterialShaderSourceBuilder::CreateEnvironment(
        EMaterialUsage::Surface,
        EShaderStage::Pixel
    );
    EXPECT_TRUE(environment.ParticleAge.empty());
    EXPECT_TRUE(environment.ParticleLifetime.empty());
}


TEST(MaterialAtlasTest, RejectsTextureParametersUsedAsAtlasResources)
{
    Material material("Wrong resource type", EMaterialUsage::Surface);
    material.DefineParameter(
        "Atlas",
        {.Kind = EMaterialParameterKind::Texture,
         .DefaultValue = SMaterialParameter::MakeTexture(nullptr)}
    );
    MaterialGraph graph;
    const auto sample = graph.AddNode<TextureAtlasSample>("Atlas");
    graph.SetChannel(EMaterialChannel::BaseColor, sample);
    material.SetGraph(std::move(graph));
    const auto result = Compiler::Build(material);
    EXPECT_FALSE(result);
    EXPECT_NE(result.Diagnostics.find("texture atlas resource"), std::string::npos);
}

TEST(MaterialAtlasTest, AtlasParametersReserveValueAndTextureSlotsAndCheckBothLimits)
{
    using namespace Elixir;
    const auto material = CreateRef<Material>("Atlas slots", EMaterialUsage::Surface);
    MaterialGraph graph;
    graph.SetChannel(EMaterialChannel::BaseColor, graph.AddNode<TextureAtlasSample>("Atlas"));
    material->SetGraph(std::move(graph));
    material->DefineParameter(
        "Atlas",
        {.Kind = EMaterialParameterKind::TextureAtlas,
         .DefaultValue = SMaterialParameter::MakeTextureAtlas(nullptr)}
    );
    auto instance = material->CreateInstance();
    EXPECT_TRUE(instance->SetTextureAtlas("Atlas", nullptr));
    EXPECT_FALSE(instance->SetTexture("Atlas", nullptr));
    EXPECT_FALSE(instance->SetScalar("Atlas", 3));
    EXPECT_FALSE(instance->GetTextureAtlas("Missing"));
    for (int index = 0; index < 31; ++index)
        ASSERT_TRUE(material->DefineParameter(
            "Value" + std::to_string(index),
            {.Kind = EMaterialParameterKind::Value,
             .ValueType = EMaterialValueType::Float,
             .DefaultValue = SMaterialParameter::MakeScalar(1)}
        ));
    ASSERT_TRUE(Compiler::Build(*material));
    material->DefineParameter(
        "OneTooMany",
        {.Kind = EMaterialParameterKind::TextureAtlas,
         .DefaultValue = SMaterialParameter::MakeTextureAtlas(nullptr)}
    );
    EXPECT_FALSE(Compiler::Build(*material));

    Material textures("Atlas texture slots", EMaterialUsage::Surface);
    for (int index = 0; index < 32; ++index)
        textures.DefineParameter(
            "Texture" + std::to_string(index),
            {.Kind = EMaterialParameterKind::Texture,
             .DefaultValue = SMaterialParameter::MakeTexture(nullptr)}
        );
    textures.DefineParameter(
        "LastAtlas",
        {.Kind = EMaterialParameterKind::TextureAtlas,
         .DefaultValue = SMaterialParameter::MakeTextureAtlas(nullptr)}
    );
    EXPECT_FALSE(Compiler::Build(textures));
}

TEST(MaterialAtlasTest, AnimationRangeComesFromTheFrameInputWhileNormalizedModeUsesTheGrid)
{
    MaterialGraph graph;
    const auto age = graph.AddNode<ParticleTime>();
    const auto lastFrame = graph.AddNode<Constant>(glm::vec4{9}, EMaterialValueType::Float);
    const auto frame = graph.AddNode<Multiply>();
    graph.Connect(age, frame, 0);
    graph.Connect(lastFrame, frame, 1);
    const auto atlas = graph.AddNode<TextureAtlasSample>("Atlas", true);
    graph.Connect(frame, atlas, 1);
    graph.SetChannel(EMaterialChannel::BaseColor, atlas);
    SMaterialGraphBindings bindings;
    bindings.Textures["Atlas"] = "3u";
    bindings.AtlasLayouts["Atlas"] = "AtlasLayout";
    const auto indexed = EmitGraph(
        graph,
        bindings,
        EShaderStage::Pixel,
        EMaterialUsage::Particle
    );
    EXPECT_NE(indexed.find("input.ParticleTime.x"), std::string::npos);
    EXPECT_NE(indexed.find("9.000000"), std::string::npos);
    EXPECT_EQ(indexed.find("TextureAtlasFrameIndex("), std::string::npos);

    const auto normalized = graph.AddNode<TextureAtlasSample>(
        "Atlas",
        true,
        ETextureAtlasFrameMode::Normalized
    );
    graph.Connect(age, normalized, 1);
    graph.SetChannel(EMaterialChannel::BaseColor, normalized);
    const auto wholeGrid = EmitGraph(
        graph,
        bindings,
        EShaderStage::Pixel,
        EMaterialUsage::Particle
    );
    EXPECT_NE(wholeGrid.find("TextureAtlasFrameIndex("), std::string::npos);
    EXPECT_EQ(wholeGrid.find("AtlasLayout.z"), std::string::npos);
}
