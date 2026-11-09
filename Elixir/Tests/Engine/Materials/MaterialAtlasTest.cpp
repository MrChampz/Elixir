#include "MaterialGraphTestEmission.h"
#include <gtest/gtest.h>
#include <Engine/Materials/Nodes/FlipbookSample.h>
#include <Engine/Materials/Nodes/ParticleTime.h>
#include <Engine/Materials/Nodes/SceneColor.h>

using namespace Elixir::Materials;
using namespace Elixir::Materials::Nodes;
using namespace Elixir::Materials::Compilation;
using namespace Elixir::Materials::Tests;

TEST(MaterialAtlasTest, AtlasSamplingAcceptsFramesFromAnyGraph)
{
    MaterialGraph graph;
    const auto atlas = graph.AddNode<FlipbookSample>("Atlas", STextureAtlas{4, 2, 8, true});
    graph.SetChannel(EMaterialChannel::BaseColor, atlas);
    SMaterialGraphBindings bindings;
    bindings.Textures["Atlas"] = "3u";
    for (const auto usage : {EMaterialUsage::Surface, EMaterialUsage::Particle})
    {
        const auto source = EmitGraph(graph, bindings, EShaderStage::Pixel, usage);
        EXPECT_NE(source.find("SampleTextureAtlas("), std::string::npos);
        EXPECT_NE(source.find("uint2(4, 2)"), std::string::npos);
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
