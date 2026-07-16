#include <gtest/gtest.h>

#include <Engine/Graphics/Material/MaterialGraph.h>
#include <algorithm>
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

// The vertex body is injected into ComputeWPO(float3 P, float2 uv), so a node that
// falls back to the mesh UV must name the one in scope for the stage. Reaching for the
// pixel shader's `input` there costs a shader compile, not a diagnostic.
TEST(MaterialGraph, UnconnectedUVFollowsTheStageInWorldPositionOffset)
{
    MaterialGraph graph;

    SMaterialNode texture;
    texture.Type = EMaterialNodeType::TextureParameter;
    texture.OutputType = EGraphValueType::Float3;
    texture.ParameterName = "Displacement";
    texture.ParamSlot = 0;
    texture.Inputs = { -1 }; // no UV wired: the node falls back to the mesh UV
    const uint32_t textureId = graph.AddNode(texture);

    graph.SetChannel(EMaterialChannel::WorldPositionOffset, textureId);
    EXPECT_FALSE(graph.Validate().HasErrors());

    // ComputeWPO takes the UV as a parameter, and each shader template defines the
    // SampleTex overload its stage can use.
    const std::string vertex = graph.GenerateHLSL(true);
    EXPECT_EQ(vertex.find("input."), std::string::npos);
    EXPECT_NE(vertex.find(", uv)"), std::string::npos);

    // The pixel stage still reads its interpolated input.
    graph.SetChannel(EMaterialChannel::BaseColor, textureId);
    EXPECT_NE(graph.GenerateHLSL(false).find("input.TexCoord"), std::string::npos);
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
    // Converted, never reinterpreted: asuint on a slot the shader templates also do
    // arithmetic on would read a NaN sentinel and a subnormal index.
    EXPECT_NE(hlsl.find("(uint)(int)GraphParams[2].x"), std::string::npos);
    EXPECT_EQ(hlsl.find("asuint(GraphParams"), std::string::npos);
    EXPECT_NE(hlsl.find("SampleTex("), std::string::npos);
    EXPECT_NE(hlsl.find("input.TexCoord"), std::string::npos);
    EXPECT_NE(hlsl.find("0xffffffffu"), std::string::npos);
}

TEST(MaterialGraph, TextureObjectParameterFeedsASeparateSampleNode)
{
    MaterialGraph graph;

    SMaterialNode object;
    object.Type = EMaterialNodeType::TextureObjectParameter;
    object.OutputType = EGraphValueType::Texture2D;
    object.ParameterName = "Albedo";
    object.ParamSlot = 3;
    const uint32_t objectId = graph.AddNode(object);

    SMaterialNode sample;
    sample.Type = EMaterialNodeType::TextureObjectSample;
    sample.OutputType = EGraphValueType::Float3;
    sample.Inputs = { -1, -1, -1 };
    const uint32_t sampleId = graph.AddNode(sample);
    graph.Connect(objectId, sampleId, 0);
    graph.SetChannel(EMaterialChannel::BaseColor, sampleId);

    EXPECT_FALSE(graph.Validate().HasErrors());
    const std::string hlsl = graph.GenerateHLSL();
    EXPECT_NE(hlsl.find("uint n1 = (uint)(int)GraphParams[3].x"), std::string::npos);
    EXPECT_NE(hlsl.find("textures[n1].Sample(texSampler, input.TexCoord)"), std::string::npos);
    EXPECT_NE(hlsl.find(").rgb"), std::string::npos);
}

TEST(MaterialGraph, TextureSampleSelectsPointClampExplicitMipAndAlpha)
{
    MaterialGraph graph;
    SMaterialNode object;
    object.Type = EMaterialNodeType::TextureObjectParameter;
    object.OutputType = EGraphValueType::Texture2D;
    object.ParameterName = "Mask";
    const uint32_t objectId = graph.AddNode(object);

    SMaterialNode sample;
    sample.Type = EMaterialNodeType::TextureObjectSample;
    sample.OutputType = EGraphValueType::Float;
    sample.Inputs = { -1, -1, -1 };
    sample.ConstantValue.x = 2.5f;
    sample.TextureSampleType = ETextureSampleType::Masks;
    sample.TextureSampleAddress = ETextureSampleAddress::Clamp;
    sample.TextureSampleFilter = ETextureSampleFilter::Point;
    sample.TextureSampleMipMode = ETextureSampleMipMode::Level;
    sample.TextureSampleOutput = ETextureSampleOutput::A;
    const uint32_t sampleId = graph.AddNode(sample);
    graph.Connect(objectId, sampleId, 0);
    graph.SetChannel(EMaterialChannel::Opacity, sampleId);

    EXPECT_FALSE(graph.Validate().HasErrors());
    const std::string hlsl = graph.GenerateHLSL();
    EXPECT_NE(hlsl.find("SampleLevel(texSamplerPointClamp, input.TexCoord, 2.500000)"), std::string::npos);
    EXPECT_NE(hlsl.find(").a"), std::string::npos);
}

TEST(MaterialGraph, TextureSampleDecodesNormalMaps)
{
    MaterialGraph graph;
    SMaterialNode object;
    object.Type = EMaterialNodeType::TextureObjectParameter;
    object.OutputType = EGraphValueType::Texture2D;
    object.ParameterName = "NormalMap";
    const uint32_t objectId = graph.AddNode(object);

    SMaterialNode sample;
    sample.Type = EMaterialNodeType::TextureObjectSample;
    sample.OutputType = EGraphValueType::Float3;
    sample.Inputs = { -1, -1, -1 };
    sample.TextureSampleType = ETextureSampleType::Normal;
    const uint32_t sampleId = graph.AddNode(sample);
    graph.Connect(objectId, sampleId, 0);
    graph.SetChannel(EMaterialChannel::Normal, sampleId);

    EXPECT_FALSE(graph.Validate().HasErrors());
    const std::string hlsl = graph.GenerateHLSL();
    EXPECT_NE(hlsl.find("float4(0.5, 0.5, 1.0, 1.0)"), std::string::npos);
    EXPECT_NE(hlsl.find("* 2.0 - 1.0"), std::string::npos);
}

TEST(MaterialGraph, TextureSampleRgbaFansOutThroughComponentMasksOnce)
{
    MaterialGraph graph;
    SMaterialNode object;
    object.Type = EMaterialNodeType::TextureObjectParameter;
    object.OutputType = EGraphValueType::Texture2D;
    object.ParameterName = "PackedMap";
    const uint32_t objectId = graph.AddNode(object);

    SMaterialNode sample;
    sample.Type = EMaterialNodeType::TextureObjectSample;
    sample.OutputType = EGraphValueType::Float4;
    sample.Inputs = { -1, -1, -1 };
    sample.TextureSampleType = ETextureSampleType::Masks;
    sample.TextureSampleOutput = ETextureSampleOutput::RGBA;
    const uint32_t sampleId = graph.AddNode(sample);
    graph.Connect(objectId, sampleId, 0);

    SMaterialNode rgb;
    rgb.Type = EMaterialNodeType::ComponentMask;
    rgb.OutputType = EGraphValueType::Float3;
    rgb.Inputs = { -1 };
    rgb.ComponentMask = 0x7;
    const uint32_t rgbId = graph.AddNode(rgb);
    graph.Connect(sampleId, rgbId, 0);

    SMaterialNode alpha;
    alpha.Type = EMaterialNodeType::ComponentMask;
    alpha.OutputType = EGraphValueType::Float;
    alpha.Inputs = { -1 };
    alpha.ComponentMask = 0x8;
    const uint32_t alphaId = graph.AddNode(alpha);
    graph.Connect(sampleId, alphaId, 0);

    graph.SetChannel(EMaterialChannel::BaseColor, rgbId);
    graph.SetChannel(EMaterialChannel::Opacity, alphaId);

    EXPECT_FALSE(graph.Validate().HasErrors());
    const std::string hlsl = graph.GenerateHLSL();
    const size_t firstSample = hlsl.find("].Sample(");
    ASSERT_NE(firstSample, std::string::npos);
    EXPECT_EQ(hlsl.find("].Sample(", firstSample + 1), std::string::npos);
    EXPECT_NE(hlsl.find("(n2).rgb"), std::string::npos);
    EXPECT_NE(hlsl.find("(n2).a"), std::string::npos);
}

TEST(MaterialGraph, AppendVectorConcatenatesInputComponents)
{
    MaterialGraph graph;
    SMaterialNode uv;
    uv.Type = EMaterialNodeType::Constant;
    uv.OutputType = EGraphValueType::Float2;
    uv.ConstantValue = { 0.25f, 0.5f, 0.0f, 0.0f };
    const uint32_t uvId = graph.AddNode(uv);

    SMaterialNode scalar;
    scalar.Type = EMaterialNodeType::Scalar;
    scalar.OutputType = EGraphValueType::Float;
    scalar.ConstantValue.x = 0.75f;
    const uint32_t scalarId = graph.AddNode(scalar);

    SMaterialNode append;
    append.Type = EMaterialNodeType::AppendVector;
    append.OutputType = EGraphValueType::Float3;
    append.Inputs = { -1, -1 };
    const uint32_t appendId = graph.AddNode(append);
    graph.Connect(uvId, appendId, 0);
    graph.Connect(scalarId, appendId, 1);
    graph.SetChannel(EMaterialChannel::BaseColor, appendId);

    EXPECT_FALSE(graph.Validate().HasErrors());
    const std::string hlsl = graph.GenerateHLSL();
    EXPECT_NE(hlsl.find("float3(n1, n2)"), std::string::npos);
}

TEST(MaterialGraph, AppendVectorRejectsMoreThanFourComponents)
{
    MaterialGraph graph;
    SMaterialNode a;
    a.Type = EMaterialNodeType::Vector;
    a.OutputType = EGraphValueType::Float3;
    const uint32_t aId = graph.AddNode(a);

    SMaterialNode b;
    b.Type = EMaterialNodeType::Constant;
    b.OutputType = EGraphValueType::Float2;
    const uint32_t bId = graph.AddNode(b);

    SMaterialNode append;
    append.SourceId = 91;
    append.Type = EMaterialNodeType::AppendVector;
    append.OutputType = EGraphValueType::Float4;
    append.Inputs = { -1, -1 };
    const uint32_t appendId = graph.AddNode(append);
    graph.Connect(aId, appendId, 0);
    graph.Connect(bId, appendId, 1);
    graph.SetChannel(EMaterialChannel::BaseColor, appendId);

    const auto validation = graph.Validate();
    ASSERT_TRUE(validation.HasErrors());
    EXPECT_TRUE(std::any_of(validation.Diagnostics.begin(), validation.Diagnostics.end(), [](const auto& diagnostic)
    {
        return diagnostic.NodeId == 91 && diagnostic.Message.find("more than four") != std::string::npos;
    }));
}

TEST(MaterialGraph, RejectsAnUnsampledTextureObjectAtTheMaterialOutput)
{
    MaterialGraph graph;
    SMaterialNode object;
    object.SourceId = 73;
    object.Type = EMaterialNodeType::TextureObjectParameter;
    object.OutputType = EGraphValueType::Texture2D;
    object.ParameterName = "Albedo";
    const uint32_t objectId = graph.AddNode(object);
    graph.SetChannel(EMaterialChannel::BaseColor, objectId);

    const auto validation = graph.Validate();
    ASSERT_TRUE(validation.HasErrors());
    EXPECT_TRUE(std::any_of(validation.Diagnostics.begin(), validation.Diagnostics.end(), [](const auto& diagnostic)
    {
        return diagnostic.NodeId == 73
            && diagnostic.Message.find("must be sampled") != std::string::npos;
    }));
}

TEST(MaterialGraph, RejectsANumericValueAtTheTextureObjectInput)
{
    MaterialGraph graph;
    SMaterialNode scalar;
    scalar.Type = EMaterialNodeType::Scalar;
    scalar.OutputType = EGraphValueType::Float;
    const uint32_t scalarId = graph.AddNode(scalar);

    SMaterialNode sample;
    sample.SourceId = 81;
    sample.Type = EMaterialNodeType::TextureObjectSample;
    sample.OutputType = EGraphValueType::Float3;
    sample.Inputs = { -1, -1, -1 };
    const uint32_t sampleId = graph.AddNode(sample);
    graph.Connect(scalarId, sampleId, 0);
    graph.SetChannel(EMaterialChannel::BaseColor, sampleId);

    const auto validation = graph.Validate();
    ASSERT_TRUE(validation.HasErrors());
    EXPECT_TRUE(std::any_of(validation.Diagnostics.begin(), validation.Diagnostics.end(), [](const auto& diagnostic)
    {
        return diagnostic.NodeId == 81
            && diagnostic.Message.find("must be a Texture Object") != std::string::npos;
    }));
}
