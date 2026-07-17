#include <gtest/gtest.h>

#include <Engine/Graphics/Material/MaterialHLSLEmitter.h>
#include <Engine/Graphics/Material/MaterialIRBuilder.h>

#include <algorithm>

using namespace Elixir;

namespace Elixir
{
    class MaterialGraphLegacyTestAccess
    {
      public:
        static std::string Generate(const MaterialGraph& graph, const bool vertexStage)
        {
            return graph.GenerateLegacyHLSL(vertexStage);
        }
    };
}

namespace
{
    SMaterialNode Constant(const EGraphValueType type, const glm::vec4 value)
    {
        SMaterialNode node;
        node.Type = EMaterialNodeType::Constant;
        node.OutputType = type;
        node.ConstantValue = value;
        return node;
    }

    bool ContainsSource(const SMaterialIR& material, const uint32_t sourceNode)
    {
        return std::ranges::any_of(material.Instructions,
            [=](const SMaterialIRInstruction& instruction)
            {
                return instruction.SourceNode == sourceNode;
            });
    }
}

TEST(MaterialIR, BuildsTypedTopologicalInstructions)
{
    MaterialGraph graph;
    const uint32_t vector = graph.AddNode(Constant(EGraphValueType::Float2,
        glm::vec4(0.25f, 0.75f, 0.0f, 0.0f)));

    SMaterialNode scalar;
    scalar.Type = EMaterialNodeType::Scalar;
    scalar.OutputType = EGraphValueType::Float;
    scalar.ConstantValue.x = 2.0f;
    const uint32_t scalarId = graph.AddNode(scalar);

    SMaterialNode multiply;
    multiply.Type = EMaterialNodeType::Multiply;
    multiply.OutputType = EGraphValueType::Float2;
    multiply.Inputs = { -1, -1 };
    const uint32_t multiplyId = graph.AddNode(multiply);
    graph.Connect(vector, multiplyId, 0);
    graph.Connect(scalarId, multiplyId, 1);
    graph.SetChannel(EMaterialChannel::BaseColor, multiplyId);

    const SMaterialIR material = MaterialIRBuilder::Build(graph, EMaterialIRStage::Pixel);
    ASSERT_EQ(material.Instructions.size(), 3u);
    EXPECT_EQ(material.Instructions[0].SourceNode, vector);
    EXPECT_EQ(material.Instructions[0].Type, EGraphValueType::Float2);
    EXPECT_EQ(material.Instructions[1].SourceNode, scalarId);
    EXPECT_EQ(material.Instructions[1].Type, EGraphValueType::Float);
    EXPECT_EQ(material.Instructions[2].SourceNode, multiplyId);
    EXPECT_EQ(material.Instructions[2].Op, EMaterialIROp::Multiply);
    EXPECT_EQ(material.Instructions[2].Type, EGraphValueType::Float2);
    ASSERT_EQ(material.Instructions[2].Inputs.size(), 2u);
    EXPECT_EQ(material.Instructions[2].Inputs[0].Type, EGraphValueType::Float2);
    EXPECT_EQ(material.Instructions[2].Inputs[1].Type, EGraphValueType::Float);
    ASSERT_EQ(material.Outputs.size(), 1u);
    EXPECT_EQ(material.Outputs[0].Channel, EMaterialChannel::BaseColor);
    EXPECT_EQ(material.Outputs[0].Value.Type, EGraphValueType::Float2);
}

TEST(MaterialIR, SeparatesPixelAndVertexStages)
{
    MaterialGraph graph;
    const uint32_t color = graph.AddNode(Constant(EGraphValueType::Float3,
        glm::vec4(0.1f, 0.2f, 0.3f, 0.0f)));
    SMaterialNode position;
    position.Type = EMaterialNodeType::Position;
    position.OutputType = EGraphValueType::Float3;
    const uint32_t positionId = graph.AddNode(position);
    graph.SetChannel(EMaterialChannel::BaseColor, color);
    graph.SetChannel(EMaterialChannel::WorldPositionOffset, positionId);

    const SMaterialIR pixel = MaterialIRBuilder::Build(graph, EMaterialIRStage::Pixel);
    const SMaterialIR vertex = MaterialIRBuilder::Build(graph, EMaterialIRStage::Vertex);

    ASSERT_EQ(pixel.Outputs.size(), 1u);
    EXPECT_EQ(pixel.Outputs[0].Channel, EMaterialChannel::BaseColor);
    EXPECT_TRUE(ContainsSource(pixel, color));
    EXPECT_FALSE(ContainsSource(pixel, positionId));

    ASSERT_EQ(vertex.Outputs.size(), 1u);
    EXPECT_EQ(vertex.Outputs[0].Channel, EMaterialChannel::WorldPositionOffset);
    EXPECT_FALSE(ContainsSource(vertex, color));
    EXPECT_TRUE(ContainsSource(vertex, positionId));
}

TEST(MaterialIR, StaticSwitchKeepsOnlyTheSelectedBranch)
{
    MaterialGraph graph;
    const uint32_t selected = graph.AddNode(Constant(EGraphValueType::Float3,
        glm::vec4(0.2f, 0.4f, 0.8f, 0.0f)));

    SMaterialNode inactive;
    inactive.Type = EMaterialNodeType::Custom;
    inactive.OutputType = EGraphValueType::Float3;
    inactive.Inputs = { -1, -1, -1 };
    inactive.CustomCode = "float3(1.0, 0.0, 1.0)";
    const uint32_t inactiveId = graph.AddNode(inactive);

    SMaterialNode condition;
    condition.Type = EMaterialNodeType::StaticBoolParameter;
    condition.OutputType = EGraphValueType::Float;
    condition.ConstantValue.x = 1.0f;
    const uint32_t conditionId = graph.AddNode(condition);

    SMaterialNode select;
    select.Type = EMaterialNodeType::StaticSwitch;
    select.OutputType = EGraphValueType::Float3;
    select.Inputs = { -1, -1, -1 };
    const uint32_t selectId = graph.AddNode(select);
    graph.Connect(selected, selectId, 0);
    graph.Connect(inactiveId, selectId, 1);
    graph.Connect(conditionId, selectId, 2);
    graph.SetChannel(EMaterialChannel::BaseColor, selectId);

    const SMaterialIR material = MaterialIRBuilder::Build(graph, EMaterialIRStage::Pixel);
    ASSERT_EQ(material.Instructions.size(), 1u);
    EXPECT_TRUE(ContainsSource(material, selected));
    EXPECT_FALSE(ContainsSource(material, inactiveId));
    EXPECT_FALSE(ContainsSource(material, conditionId));
    EXPECT_FALSE(ContainsSource(material, selectId));
}

TEST(MaterialIR, HlslEmitterMatchesTheLegacyGenerator)
{
    MaterialGraph graph;
    graph.SetBlend(EMaterialBlendMode::Masked, 0.375f);

    SMaterialNode texture;
    texture.Type = EMaterialNodeType::TextureObjectParameter;
    texture.OutputType = EGraphValueType::Texture2D;
    texture.ParameterName = "PackedTexture";
    texture.ParamSlot = 2;
    const uint32_t textureId = graph.AddNode(texture);

    SMaterialNode uv;
    uv.Type = EMaterialNodeType::Panner;
    uv.OutputType = EGraphValueType::Float2;
    uv.Inputs = { -1 };
    uv.ConstantValue = glm::vec4(0.2f, -0.1f, 0.0f, 0.0f);
    const uint32_t uvId = graph.AddNode(uv);

    SMaterialNode mip;
    mip.Type = EMaterialNodeType::Scalar;
    mip.OutputType = EGraphValueType::Float;
    mip.ConstantValue.x = 1.5f;
    const uint32_t mipId = graph.AddNode(mip);

    SMaterialNode sample;
    sample.Type = EMaterialNodeType::TextureObjectSample;
    sample.OutputType = EGraphValueType::Float4;
    sample.Inputs = { -1, -1, -1 };
    sample.TextureSampleType = ETextureSampleType::Normal;
    sample.TextureSampleAddress = ETextureSampleAddress::Clamp;
    sample.TextureSampleFilter = ETextureSampleFilter::Point;
    sample.TextureSampleMipMode = ETextureSampleMipMode::Bias;
    sample.TextureSampleOutput = ETextureSampleOutput::RGBA;
    const uint32_t sampleId = graph.AddNode(sample);
    graph.Connect(textureId, sampleId, 0);
    graph.Connect(uvId, sampleId, 1);
    graph.Connect(mipId, sampleId, 2);

    SMaterialNode mask;
    mask.Type = EMaterialNodeType::ComponentMask;
    mask.OutputType = EGraphValueType::Float3;
    mask.Inputs = { -1 };
    mask.ComponentMask = 0x7;
    const uint32_t maskId = graph.AddNode(mask);
    graph.Connect(sampleId, maskId, 0);

    SMaterialNode custom;
    custom.Type = EMaterialNodeType::Custom;
    custom.OutputType = EGraphValueType::Float3;
    custom.Inputs = { -1, -1, -1 };
    custom.CustomCode = "saturate(a * 0.5 + b.xxx)";
    const uint32_t customId = graph.AddNode(custom);
    graph.Connect(maskId, customId, 0);
    graph.Connect(mipId, customId, 1);

    SMaterialNode opacity;
    opacity.Type = EMaterialNodeType::Saturate;
    opacity.OutputType = EGraphValueType::Float;
    opacity.Inputs = { -1 };
    const uint32_t opacityId = graph.AddNode(opacity);
    graph.Connect(mipId, opacityId, 0);

    SMaterialNode position;
    position.Type = EMaterialNodeType::Position;
    position.OutputType = EGraphValueType::Float3;
    const uint32_t positionId = graph.AddNode(position);

    SMaterialNode noise;
    noise.Type = EMaterialNodeType::Noise;
    noise.OutputType = EGraphValueType::Float;
    noise.Inputs = { -1 };
    noise.ConstantValue.x = 3.0f;
    const uint32_t noiseId = graph.AddNode(noise);
    graph.Connect(positionId, noiseId, 0);

    graph.SetChannel(EMaterialChannel::BaseColor, customId);
    graph.SetChannel(EMaterialChannel::Opacity, opacityId);
    graph.SetChannel(EMaterialChannel::WorldPositionOffset, noiseId);

    for (const bool vertexStage : { false, true })
    {
        const EMaterialIRStage stage = vertexStage
            ? EMaterialIRStage::Vertex : EMaterialIRStage::Pixel;
        const SMaterialIR material = MaterialIRBuilder::Build(graph, stage);
        const std::string emitted = MaterialHLSLEmitter::Emit(material);
        EXPECT_EQ(emitted, MaterialGraphLegacyTestAccess::Generate(graph, vertexStage));
        EXPECT_EQ(graph.GenerateHLSL(vertexStage), emitted);
    }
}
