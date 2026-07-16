#include <gtest/gtest.h>

#include <Engine/Graphics/Material/MaterialGraph.h>
#include <iostream>

using namespace Elixir;

// BaseColor = Constant([1,0,0,1]) * Parameter(BaseColorFactor)
TEST(MaterialGraph, GeneratesMultiplyBaseColor)
{
    MaterialGraph graph;

    SMaterialNode constant;
    constant.Type = EMaterialNodeType::Constant;
    constant.OutputType = EGraphValueType::Float4;
    constant.ConstantValue = { 1.0f, 0.0f, 0.0f, 1.0f };
    const uint32_t c = graph.AddNode(constant);

    SMaterialNode param;
    param.Type = EMaterialNodeType::Parameter;
    param.OutputType = EGraphValueType::Float4;
    param.ParameterName = "BaseColorFactor";
    const uint32_t p = graph.AddNode(param);

    SMaterialNode mul;
    mul.Type = EMaterialNodeType::Multiply;
    mul.OutputType = EGraphValueType::Float4;
    mul.Inputs = { -1, -1 };
    const uint32_t m = graph.AddNode(mul);

    graph.Connect(c, m, 0);
    graph.Connect(p, m, 1);
    graph.SetChannel(EMaterialChannel::BaseColor, m);

    const std::string hlsl = graph.GenerateHLSL();
    std::cout << "\n--- Generated HLSL (BaseColor) ---\n" << hlsl << "----------------------------------\n";

    EXPECT_NE(hlsl.find("mat.BaseColorFactor"), std::string::npos);
    EXPECT_NE(hlsl.find("float4(1"), std::string::npos);
    EXPECT_NE(hlsl.find(" * "), std::string::npos);
    EXPECT_NE(hlsl.find("surface.BaseColor ="), std::string::npos);
    EXPECT_NE(hlsl.find(").rgb"), std::string::npos); // float4 coerced to the float3 channel
}

// Scalar channels coerce and a shared node is emitted once.
TEST(MaterialGraph, ScalarChannelsAndSharedNode)
{
    MaterialGraph graph;

    SMaterialNode metallic;
    metallic.Type = EMaterialNodeType::Constant;
    metallic.OutputType = EGraphValueType::Float;
    metallic.ConstantValue = { 0.5f, 0.0f, 0.0f, 0.0f };
    const uint32_t mNode = graph.AddNode(metallic);

    graph.SetChannel(EMaterialChannel::Metallic, mNode);
    graph.SetChannel(EMaterialChannel::Roughness, mNode);

    const std::string hlsl = graph.GenerateHLSL();
    std::cout << "\n--- Generated HLSL (scalars) ---\n" << hlsl << "--------------------------------\n";

    EXPECT_NE(hlsl.find("surface.Metallic ="), std::string::npos);
    EXPECT_NE(hlsl.find("surface.Roughness ="), std::string::npos);
    // The shared constant node should be declared exactly once.
    const auto first = hlsl.find("float n");
    ASSERT_NE(first, std::string::npos);
    EXPECT_EQ(hlsl.find("float n", first + 1), std::string::npos);
}

TEST(MaterialGraph, ValidatesReachableBrokenLinks)
{
    MaterialGraph graph;

    SMaterialNode add;
    add.SourceId = 42;
    add.Type = EMaterialNodeType::Add;
    add.Inputs = { -1, -1 };
    const uint32_t node = graph.AddNode(add);
    graph.Connect(9999, node, 0);
    graph.SetChannel(EMaterialChannel::BaseColor, node);

    const auto validation = graph.Validate();
    ASSERT_TRUE(validation.HasErrors());
    ASSERT_FALSE(validation.Diagnostics.empty());
    EXPECT_EQ(validation.Diagnostics.front().NodeId, 42u);
    EXPECT_NE(validation.Diagnostics.front().Message.find("missing node"), std::string::npos);
}

TEST(MaterialGraph, DetectsCyclesAndKeepsCodegenSafe)
{
    MaterialGraph graph;

    SMaterialNode add;
    add.SourceId = 10;
    add.Type = EMaterialNodeType::Add;
    add.Inputs = { -1, -1 };
    const uint32_t a = graph.AddNode(add);

    add.SourceId = 11;
    const uint32_t b = graph.AddNode(add);
    graph.Connect(a, b, 0);
    graph.Connect(b, a, 0);
    graph.SetChannel(EMaterialChannel::BaseColor, a);

    const auto validation = graph.Validate();
    EXPECT_TRUE(validation.HasErrors());

    bool sawA = false, sawB = false;
    for (const auto& diagnostic : validation.Diagnostics)
    {
        if (diagnostic.Message.find("Cycle") == std::string::npos)
            continue;
        sawA |= diagnostic.NodeId == 10;
        sawB |= diagnostic.NodeId == 11;
    }
    EXPECT_TRUE(sawA);
    EXPECT_TRUE(sawB);
    EXPECT_FALSE(graph.GenerateHLSL().empty());
}

TEST(MaterialGraph, IgnoresInvalidDisconnectedWorkInProgress)
{
    MaterialGraph graph;

    SMaterialNode color;
    color.Type = EMaterialNodeType::Constant;
    color.OutputType = EGraphValueType::Float3;
    const uint32_t output = graph.AddNode(color);
    graph.SetChannel(EMaterialChannel::BaseColor, output);

    SMaterialNode disconnected;
    disconnected.Type = EMaterialNodeType::Parameter;
    disconnected.ParameterName = "not a valid identifier";
    graph.AddNode(disconnected);

    EXPECT_FALSE(graph.Validate().HasErrors());
}

TEST(MaterialGraph, RejectsPixelOnlyNodeInWorldPositionOffset)
{
    MaterialGraph graph;

    SMaterialNode fresnel;
    fresnel.SourceId = 77;
    fresnel.Type = EMaterialNodeType::Fresnel;
    fresnel.OutputType = EGraphValueType::Float;
    const uint32_t node = graph.AddNode(fresnel);
    graph.SetChannel(EMaterialChannel::WorldPositionOffset, node);

    const auto validation = graph.Validate();
    ASSERT_TRUE(validation.HasErrors());
    EXPECT_EQ(validation.Diagnostics.front().NodeId, 77u);
    EXPECT_NE(validation.Diagnostics.front().Message.find("World Position Offset"), std::string::npos);
}

TEST(MaterialGraph, StaticSwitchEmitsOnlyTheSelectedBranch)
{
    MaterialGraph graph;

    SMaterialNode selected;
    selected.Type = EMaterialNodeType::Constant;
    selected.OutputType = EGraphValueType::Float3;
    selected.ConstantValue = glm::vec4(0.25f, 0.5f, 0.75f, 1.0f);
    const uint32_t selectedId = graph.AddNode(selected);

    SMaterialNode inactive;
    inactive.Type = EMaterialNodeType::Parameter;
    inactive.OutputType = EGraphValueType::Float3;
    inactive.ParameterName = "invalid inactive parameter";
    const uint32_t inactiveId = graph.AddNode(inactive);

    SMaterialNode condition;
    condition.Type = EMaterialNodeType::StaticBoolParameter;
    condition.OutputType = EGraphValueType::Float;
    condition.ParameterName = "UseSelected";
    condition.ConstantValue.x = 1.0f;
    const uint32_t conditionId = graph.AddNode(condition);

    SMaterialNode staticSwitch;
    staticSwitch.SourceId = 91;
    staticSwitch.Type = EMaterialNodeType::StaticSwitch;
    staticSwitch.Inputs = { -1, -1, -1 };
    const uint32_t switchId = graph.AddNode(staticSwitch);
    graph.Connect(selectedId, switchId, 0);
    graph.Connect(inactiveId, switchId, 1);
    graph.Connect(conditionId, switchId, 2);
    graph.SetChannel(EMaterialChannel::BaseColor, switchId);

    EXPECT_FALSE(graph.Validate().HasErrors());
    const std::string hlsl = graph.GenerateHLSL();
    EXPECT_NE(hlsl.find("0.250000"), std::string::npos);
    EXPECT_EQ(hlsl.find("invalid inactive parameter"), std::string::npos);
}

TEST(MaterialGraph, TextureParameterSamplesTheBindlessGraphParameter)
{
    MaterialGraph graph;

    SMaterialNode texture;
    texture.Type = EMaterialNodeType::TextureParameter;
    texture.OutputType = EGraphValueType::Float3;
    texture.ParameterName = "Albedo";
    texture.ParamSlot = 2;
    texture.Inputs = { -1 };
    const uint32_t textureId = graph.AddNode(texture);
    graph.SetChannel(EMaterialChannel::BaseColor, textureId);

    EXPECT_FALSE(graph.Validate().HasErrors());
    const std::string hlsl = graph.GenerateHLSL();
    EXPECT_NE(hlsl.find("asuint(GraphParams[2].x)"), std::string::npos);
    EXPECT_NE(hlsl.find("SampleTex("), std::string::npos);
    EXPECT_NE(hlsl.find("input.TexCoord"), std::string::npos);
    EXPECT_NE(hlsl.find("0xffffffffu"), std::string::npos);
}
