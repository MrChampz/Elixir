#include <gtest/gtest.h>

#include <Engine/Material/MaterialGraph.h>

using namespace Elixir;

// BaseColor = Constant([1,0,0,1]) * Parameter(BaseColorFactor)
TEST(MaterialGraphTest, GeneratesMultiplyBaseColor)
{
    MaterialGraph graph;

    SMaterialNode constant;
    constant.Type = EMaterialNodeType::Constant;
    constant.OutputType = EMaterialGraphValueType::Float4;
    constant.ConstantValue = { 1.0f, 0.0f, 0.0f, 1.0f };
    const uint32_t constantNodeId = graph.AddNode(constant);

    SMaterialNode param;
    param.Type = EMaterialNodeType::Parameter;
    param.OutputType = EMaterialGraphValueType::Float4;
    param.ParameterName = "BaseColorFactor";
    const uint32_t paramNodeId = graph.AddNode(param);

    SMaterialNode mul;
    mul.Type = EMaterialNodeType::Multiply;
    mul.OutputType = EMaterialGraphValueType::Float4;
    mul.Inputs = { -1, -1 };
    const uint32_t mulNodeId = graph.AddNode(mul);

    graph.Connect(constantNodeId, mulNodeId, 0);
    graph.Connect(paramNodeId, mulNodeId, 1);
    graph.SetChannel(EMaterialChannel::BaseColor, mulNodeId);

    const std::string hlsl = graph.GenerateHLSL();
    std::cout
        << "\n--- Generated HLSL (BaseColor) ---\n"
        << hlsl
        << "----------------------------------\n";

    EXPECT_NE(hlsl.find("mat.BaseColorFactor"), std::string::npos);
    EXPECT_NE(hlsl.find("float4(1"), std::string::npos);
    EXPECT_NE(hlsl.find(" * "), std::string::npos);
    EXPECT_NE(hlsl.find("surface.BaseColor ="), std::string::npos);
    EXPECT_NE(hlsl.find(").rgb"), std::string::npos);  // float4 coerced to the float3 channel
}

// Scalar channels coerce and a shared node is emitted once.
TEST(MaterialGraphTest, ScalarChannelsAndSharedNode)
{
    MaterialGraph graph;

    SMaterialNode metallic;
    metallic.Type = EMaterialNodeType::Constant;
    metallic.OutputType = EMaterialGraphValueType::Float;
    metallic.ConstantValue = { 0.5f, 0.0f, 0.0f, 0.0f };
    const uint32_t metallicNodeId = graph.AddNode(metallic);

    graph.SetChannel(EMaterialChannel::Metallic, metallicNodeId);
    graph.SetChannel(EMaterialChannel::Roughness, metallicNodeId);

    const std::string hlsl = graph.GenerateHLSL();
    std::cout
    << "\n--- Generated HLSL (scalars) ---\n"
    << hlsl
    << "----------------------------------\n";

    EXPECT_NE(hlsl.find("surface.Metallic ="), std::string::npos);
    EXPECT_NE(hlsl.find("surface.Roughness ="), std::string::npos);

    // The shared constant node should be declared exactly once.
    const auto first = hlsl.find("float n");
    ASSERT_NE(first, std::string::npos);
    EXPECT_EQ(hlsl.find("float n", first + 1), std::string::npos);
}