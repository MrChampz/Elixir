#include <gtest/gtest.h>

#include <Engine/Material/MaterialGraph.h>
#include <Engine/Material/Nodes/BinaryOperationNode.h>
#include <Engine/Material/Nodes/ComponentMaskNode.h>
#include <Engine/Material/Nodes/ConstantNode.h>
#include <Engine/Material/Nodes/ParameterNode.h>
#include <Engine/Material/Nodes/RadialGradientExponentialNode.h>
#include <Engine/Material/Nodes/TextureSampleNode.h>

using namespace Elixir;

TEST(MaterialGraphTest, GeneratesMultiplyBaseColor)
{
    MaterialGraph graph;
    const auto constant = graph.AddNode<MaterialNodes::ConstantNode>(
        glm::vec4{ 1.0f, 0.0f, 0.0f, 1.0f }, EMaterialValueType::Float4);
    const auto parameter = graph.AddNode<MaterialNodes::ParameterNode>(
        "BaseColorFactor", EMaterialValueType::Float4);
    const auto multiply = graph.AddNode<MaterialNodes::BinaryOperationNode>(
        MaterialNodes::EBinaryMaterialOperation::Multiply);
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

TEST(MaterialGraphTest, ScalarChannelsAndSharedNode)
{
    MaterialGraph graph;
    const auto metallic = graph.AddNode<MaterialNodes::ConstantNode>(
        glm::vec4{ 0.5f, 0.0f, 0.0f, 0.0f }, EMaterialValueType::Float);
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
    const auto texture = graph.AddNode<MaterialNodes::TextureSampleNode>("Albedo");
    const auto alpha = graph.AddNode<MaterialNodes::ComponentMaskNode>(3);
    graph.Connect(texture, alpha, 0);
    graph.SetChannel(EMaterialChannel::BaseColor, texture);
    graph.SetChannel(EMaterialChannel::Opacity, alpha);

    const auto hlsl = graph.GenerateHLSL({ .Textures = {{ "Albedo", "mat.TextureIndices[0]" }} });
    EXPECT_NE(hlsl.find("surface.BaseColor"), std::string::npos);
    EXPECT_NE(hlsl.find("surface.Opacity"), std::string::npos);
    EXPECT_NE(hlsl.find("SampleTex"), std::string::npos);
    EXPECT_NE(hlsl.find(".w"), std::string::npos);
}

TEST(MaterialGraphTest, GeneratesExponentialRadialGradientForOpacity)
{
    MaterialGraph graph;
    const auto gradient = graph.AddNode<MaterialNodes::RadialGradientExponentialNode>(
        glm::vec2{ 0.25f, 0.75f }, 0.4f, 3.0f);
    graph.SetChannel(EMaterialChannel::Opacity, gradient);

    const auto hlsl = graph.GenerateHLSL();
    EXPECT_NE(hlsl.find("length((input.TexCoord - float2(0.250000, 0.750000)) / 0.400000)"), std::string::npos);
    EXPECT_NE(hlsl.find("pow(saturate(1.0 -"), std::string::npos);
    EXPECT_NE(hlsl.find(", 3.000000)"), std::string::npos);
    EXPECT_NE(hlsl.find("surface.Opacity ="), std::string::npos);
}
