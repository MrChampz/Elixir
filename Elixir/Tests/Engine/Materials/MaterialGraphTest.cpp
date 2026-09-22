#include <gtest/gtest.h>

#include <Engine/Materials/MaterialGraph.h>

#include <Engine/Materials/Nodes/Multiply.h>
#include <Engine/Materials/Nodes/ComponentMask.h>
#include <Engine/Materials/Nodes/Constant.h>
#include <Engine/Materials/Nodes/FlattenNormal.h>
#include <Engine/Materials/Nodes/Lerp.h>
#include <Engine/Materials/Nodes/Parameter.h>
#include <Engine/Materials/Nodes/RadialGradientExponential.h>
#include <Engine/Materials/Nodes/TextureSample.h>

using namespace Elixir;
using namespace Elixir::Materials;
using namespace Elixir::Materials::Nodes;

// BaseColor = Constant([1,0,0,1]) * Parameter(BaseColorFactor)
TEST(MaterialGraphTest, GeneratesMultiplyBaseColor)
{
    MaterialGraph graph;

    const auto constant = graph.AddNode<Constant>(
        glm::vec4{ 1.0f, 0.0f, 0.0f, 1.0f },
        EMaterialValueType::Float4
    );
    const auto parameter = graph.AddNode<Parameter>(
        "BaseColorFactor",
        EMaterialValueType::Float4
    );
    const auto multiply = graph.AddNode<Multiply>();
    graph.Connect(constant, multiply, 0);
    graph.Connect(parameter, multiply, 1);
    graph.SetChannel(EMaterialChannel::BaseColor, multiply);

    const auto hlsl = graph.GenerateHLSL();

    EXPECT_NE(hlsl.find("mat.BaseColorFactor"), std::string::npos);
    EXPECT_NE(hlsl.find("float4(1"), std::string::npos);
    EXPECT_NE(hlsl.find(" * "), std::string::npos);
    EXPECT_NE(hlsl.find("surface.BaseColor ="), std::string::npos);
    EXPECT_NE(hlsl.find(").rgb"), std::string::npos);
}

// Scalar channels coerce and a shared node is emitted once.
TEST(MaterialGraphTest, ScalarChannelsAndSharedNode)
{
    MaterialGraph graph;

    const auto metallic = graph.AddNode<Constant>(
        glm::vec4{ 0.5f, 0.0f, 0.0f, 0.0f },
        EMaterialValueType::Float
    );
    graph.SetChannel(EMaterialChannel::Metallic, metallic);
    graph.SetChannel(EMaterialChannel::Roughness, metallic);

    const auto hlsl = graph.GenerateHLSL();

    EXPECT_NE(hlsl.find("surface.Metallic ="), std::string::npos);
    EXPECT_NE(hlsl.find("surface.Roughness ="), std::string::npos);

    const auto first = hlsl.find("float n");
    ASSERT_NE(first, std::string::npos);
    EXPECT_EQ(hlsl.find("float n", first + 1), std::string::npos);
}

TEST(MaterialGraphTest, RoutesTextureAlphaToOpacity)
{
    MaterialGraph graph;
    const auto texture = graph.AddNode<TextureSample>("Albedo");
    const auto alpha = graph.AddNode<ComponentMask>(3);
    graph.Connect(texture, alpha, 0);
    graph.SetChannel(EMaterialChannel::BaseColor, texture);
    graph.SetChannel(EMaterialChannel::Opacity, alpha);

    const auto hlsl = graph.GenerateHLSL({ .Textures = {{ "Albedo", "mat.TextureIndices[0]" }} });
    EXPECT_NE(hlsl.find("surface.BaseColor"), std::string::npos);
    EXPECT_NE(hlsl.find("surface.Opacity"), std::string::npos);
    EXPECT_NE(hlsl.find("SampleTex"), std::string::npos);
    EXPECT_NE(hlsl.find(".w"), std::string::npos);
}

TEST(MaterialGraphTest, GeneratesNormalTextureSamplingAndFlattening)
{
    MaterialGraph graph;

    const auto texture = graph.AddNode<TextureSample>(
        "NormalTexture",
        ETextureSampleType::Normal
    );
    const auto flatness = graph.AddNode<Parameter>(
        "NormalScale",
        EMaterialValueType::Float
    );
    const auto flatten = graph.AddNode<FlattenNormal>();
    graph.Connect(texture, flatten, 0);
    graph.Connect(flatness, flatten, 1);
    graph.SetChannel(EMaterialChannel::Normal, flatten);

    const auto hlsl = graph.GenerateHLSL({
        .Values = {{ "NormalScale", "mat.Values[0].x" }},
        .Textures = {{ "NormalTexture", "mat.TextureIndices[0]" }}
    });

    EXPECT_NE(hlsl.find("SampleNormal(mat.TextureIndices[0], input.TexCoord)"), std::string::npos);
    EXPECT_NE(hlsl.find("lerp(1.0"), std::string::npos);
    EXPECT_NE(hlsl.find("surface.Normal ="), std::string::npos);
}

TEST(MaterialGraphTest, InterpolatesAmbientOcclusionFromOne)
{
    MaterialGraph graph;

    const auto one = graph.AddNode<Constant>(
        glm::vec4{ 1.0f },
        EMaterialValueType::Float
    );
    const auto occlusion = graph.AddNode<Parameter>(
        "Occlusion",
        EMaterialValueType::Float
    );
    const auto strength = graph.AddNode<Parameter>(
        "OcclusionStrength",
        EMaterialValueType::Float
    );
    const auto lerp = graph.AddNode<Lerp>();
    graph.Connect(one, lerp, 0);
    graph.Connect(occlusion, lerp, 1);
    graph.Connect(strength, lerp, 2);
    graph.SetChannel(EMaterialChannel::AmbientOcclusion, lerp);

    const auto hlsl = graph.GenerateHLSL({
        .Values = {
            { "Occlusion", "mat.Values[0].x" },
            { "OcclusionStrength", "mat.Values[1].x" }
        }
    });

    EXPECT_NE(hlsl.find("lerp(1.000000"), std::string::npos);
    EXPECT_NE(hlsl.find("surface.AmbientOcclusion ="), std::string::npos);
}

TEST(MaterialGraphTest, GeneratesExponentialRadialGradientForOpacity)
{
    MaterialGraph graph;

    const auto gradient = graph.AddNode<RadialGradientExponential>(
        glm::vec2{ 0.25f, 0.75f },
        0.4f,
        3.0f
    );
    graph.SetChannel(EMaterialChannel::Opacity, gradient);

    const auto hlsl = graph.GenerateHLSL();

    EXPECT_NE(hlsl.find("length((input.TexCoord - float2(0.250000, 0.750000)) / 0.400000)"), std::string::npos);
    EXPECT_NE(hlsl.find("pow(saturate(1.0 -"), std::string::npos);
    EXPECT_NE(hlsl.find(", 3.000000)"), std::string::npos);
    EXPECT_NE(hlsl.find("surface.Opacity ="), std::string::npos);
}
