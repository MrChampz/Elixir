#include "epch.h"
#include "MaterialIRBuilder.h"

#include <bit>
#include <unordered_map>
#include <unordered_set>

namespace Elixir
{
    namespace
    {
        int Components(const EGraphValueType type)
        {
            switch (type)
            {
                case EGraphValueType::Float: return 1;
                case EGraphValueType::Float2: return 2;
                case EGraphValueType::Float3: return 3;
                case EGraphValueType::Float4: return 4;
                case EGraphValueType::Texture2D: return 0;
            }
            return 4;
        }

        EGraphValueType TypeFromComponents(const int components)
        {
            switch (components)
            {
                case 1: return EGraphValueType::Float;
                case 2: return EGraphValueType::Float2;
                case 3: return EGraphValueType::Float3;
                default: return EGraphValueType::Float4;
            }
        }

        EGraphValueType Wider(const EGraphValueType a, const EGraphValueType b)
        {
            return Components(a) >= Components(b) ? a : b;
        }

        EMaterialIROp ToIROp(const EMaterialNodeType type)
        {
            switch (type)
            {
                case EMaterialNodeType::Constant:
                case EMaterialNodeType::Vector: return EMaterialIROp::Constant;
                case EMaterialNodeType::Scalar: return EMaterialIROp::Scalar;
                case EMaterialNodeType::Bool:
                case EMaterialNodeType::StaticBoolParameter: return EMaterialIROp::Bool;
                case EMaterialNodeType::ParamScalar: return EMaterialIROp::GraphParameterScalar;
                case EMaterialNodeType::ParamColor: return EMaterialIROp::GraphParameterColor;
                case EMaterialNodeType::Parameter: return EMaterialIROp::MaterialParameter;
                case EMaterialNodeType::TextureParameter: return EMaterialIROp::TextureParameter;
                case EMaterialNodeType::TextureObjectParameter: return EMaterialIROp::TextureObjectParameter;
                case EMaterialNodeType::TextureObjectSample: return EMaterialIROp::TextureObjectSample;
                case EMaterialNodeType::TextureSample: return EMaterialIROp::LegacyTextureSample;
                case EMaterialNodeType::TexCoord: return EMaterialIROp::TexCoord;
                case EMaterialNodeType::Position: return EMaterialIROp::Position;
                case EMaterialNodeType::Time: return EMaterialIROp::Time;
                case EMaterialNodeType::Sine: return EMaterialIROp::Sine;
                case EMaterialNodeType::Panner: return EMaterialIROp::Panner;
                case EMaterialNodeType::Checker: return EMaterialIROp::Checker;
                case EMaterialNodeType::Noise: return EMaterialIROp::Noise;
                case EMaterialNodeType::Multiply: return EMaterialIROp::Multiply;
                case EMaterialNodeType::Add: return EMaterialIROp::Add;
                case EMaterialNodeType::Subtract: return EMaterialIROp::Subtract;
                case EMaterialNodeType::Divide: return EMaterialIROp::Divide;
                case EMaterialNodeType::Power: return EMaterialIROp::Power;
                case EMaterialNodeType::Dot: return EMaterialIROp::Dot;
                case EMaterialNodeType::Lerp: return EMaterialIROp::Lerp;
                case EMaterialNodeType::OneMinus: return EMaterialIROp::OneMinus;
                case EMaterialNodeType::Saturate: return EMaterialIROp::Saturate;
                case EMaterialNodeType::Fresnel: return EMaterialIROp::Fresnel;
                case EMaterialNodeType::Custom: return EMaterialIROp::Custom;
                case EMaterialNodeType::StaticSwitch: return EMaterialIROp::StaticSwitch;
                case EMaterialNodeType::ComponentMask: return EMaterialIROp::ComponentMask;
                case EMaterialNodeType::AppendVector: return EMaterialIROp::AppendVector;
                case EMaterialNodeType::FunctionInput:
                case EMaterialNodeType::FunctionCall: return EMaterialIROp::FunctionFallback;
            }
            return EMaterialIROp::FunctionFallback;
        }

        class SBuilder
        {
          public:
            SBuilder(const MaterialGraph& graph, const EMaterialIRStage stage)
                : m_Graph(graph)
                , m_Nodes(graph.GetNodes())
            {
                m_IR.Stage = stage;
                m_IR.Domain = graph.GetDomain();
                m_IR.Usages = graph.GetUsages();
                m_IR.BlendMode = graph.GetBlendMode();
                m_IR.AlphaCutoff = graph.GetAlphaCutoff();
            }

            SMaterialIR Build()
            {
                for (uint8_t channel = 0; channel <= (uint8_t)EMaterialChannel::Sheen; ++channel)
                {
                    const auto output = m_Graph.GetChannels().find(channel);
                    if (output == m_Graph.GetChannels().end())
                        continue;
                    const auto materialChannel = (EMaterialChannel)channel;
                    const bool isVertexOutput = materialChannel == EMaterialChannel::WorldPositionOffset;
                    if (isVertexOutput != (m_IR.Stage == EMaterialIRStage::Vertex))
                        continue;
                    const SMaterialIRValue value = Resolve(output->second);
                    m_IR.Outputs.push_back({ materialChannel, value, m_IR.Instructions.size() });
                }
                return std::move(m_IR);
            }

          private:
            SMaterialIRValue Resolve(const uint32_t nodeId)
            {
                if (const auto value = m_Values.find(nodeId); value != m_Values.end())
                    return value->second;
                if (!m_Visiting.insert(nodeId).second)
                    return { SMaterialIRValue::INLINE, EGraphValueType::Float, "0.0" };

                const auto nodeIt = m_Nodes.find(nodeId);
                if (nodeIt == m_Nodes.end())
                {
                    m_Visiting.erase(nodeId);
                    return { SMaterialIRValue::INLINE, EGraphValueType::Float4, "0.0" };
                }
                const SMaterialNode& node = nodeIt->second;

                // Compile-time switches are aliases, not runtime instructions. Only
                // the chosen branch contributes dependencies to this permutation.
                if (node.Type == EMaterialNodeType::StaticSwitch
                    && node.Inputs.size() >= 3 && node.Inputs[2] >= 0)
                {
                    const auto condition = m_Nodes.find((uint32_t)node.Inputs[2]);
                    if (condition != m_Nodes.end()
                        && (condition->second.Type == EMaterialNodeType::StaticBoolParameter
                            || condition->second.Type == EMaterialNodeType::Bool))
                    {
                        const size_t choice = condition->second.ConstantValue.x >= 0.5f ? 0u : 1u;
                        const int32_t selected = node.Inputs[choice];
                        const SMaterialIRValue value = selected >= 0
                            ? Resolve((uint32_t)selected)
                            : SMaterialIRValue{ SMaterialIRValue::INLINE,
                                EGraphValueType::Float, "0.0" };
                        m_Values[nodeId] = value;
                        m_Visiting.erase(nodeId);
                        return value;
                    }
                }

                std::vector<SMaterialIRValue> inputs;
                inputs.reserve(node.Inputs.size());
                for (size_t input = 0; input < node.Inputs.size(); ++input)
                {
                    if (node.Inputs[input] >= 0)
                        inputs.push_back(Resolve((uint32_t)node.Inputs[input]));
                    else if (input < node.DefaultInputs.size())
                        inputs.push_back({ SMaterialIRValue::INLINE,
                            EGraphValueType::Float, node.DefaultInputs[input] });
                    else
                        inputs.push_back({ SMaterialIRValue::INLINE,
                            EGraphValueType::Float, "0.0" });
                }

                SMaterialIRInstruction instruction;
                instruction.SourceNode = nodeId;
                instruction.Op = ToIROp(node.Type);
                instruction.Type = InferType(node, inputs);
                instruction.Inputs = std::move(inputs);
                instruction.InputConnected.reserve(node.Inputs.size());
                for (const int32_t input : node.Inputs)
                    instruction.InputConnected.push_back(input >= 0);
                instruction.ConstantValue = node.ConstantValue;
                instruction.Name = node.ParameterName;
                instruction.CustomCode = node.CustomCode;
                instruction.LegacyTextureExpression = node.TextureExpression;
                instruction.ParameterSlot = node.ParamSlot;
                instruction.TextureSampleType = node.TextureSampleType;
                instruction.TextureSampleAddress = node.TextureSampleAddress;
                instruction.TextureSampleFilter = node.TextureSampleFilter;
                instruction.TextureSampleMipMode = node.TextureSampleMipMode;
                instruction.TextureSampleOutput = node.TextureSampleOutput;
                instruction.ComponentMask = node.ComponentMask;

                const uint32_t instructionIndex = (uint32_t)m_IR.Instructions.size();
                m_IR.Instructions.push_back(std::move(instruction));
                const SMaterialIRValue value{ instructionIndex,
                    m_IR.Instructions.back().Type, {} };
                m_Values[nodeId] = value;
                m_Visiting.erase(nodeId);
                return value;
            }

            static EGraphValueType InferType(
                const SMaterialNode& node, const std::vector<SMaterialIRValue>& inputs)
            {
                const auto inputType = [&](const size_t index)
                {
                    return index < inputs.size() ? inputs[index].Type : EGraphValueType::Float;
                };

                switch (node.Type)
                {
                    case EMaterialNodeType::Constant:
                    case EMaterialNodeType::Vector:
                    case EMaterialNodeType::Custom:
                    case EMaterialNodeType::Parameter: return node.OutputType;
                    case EMaterialNodeType::Scalar:
                    case EMaterialNodeType::Bool:
                    case EMaterialNodeType::StaticBoolParameter:
                    case EMaterialNodeType::ParamScalar:
                    case EMaterialNodeType::Checker:
                    case EMaterialNodeType::Noise:
                    case EMaterialNodeType::Dot:
                    case EMaterialNodeType::Fresnel: return EGraphValueType::Float;
                    case EMaterialNodeType::ParamColor: return EGraphValueType::Float4;
                    case EMaterialNodeType::TextureParameter:
                    case EMaterialNodeType::TextureSample: return EGraphValueType::Float3;
                    case EMaterialNodeType::TextureObjectParameter: return EGraphValueType::Texture2D;
                    case EMaterialNodeType::TextureObjectSample:
                        if (node.TextureSampleOutput == ETextureSampleOutput::RGBA)
                            return EGraphValueType::Float4;
                        if (node.TextureSampleOutput == ETextureSampleOutput::RGB)
                            return EGraphValueType::Float3;
                        return EGraphValueType::Float;
                    case EMaterialNodeType::ComponentMask:
                    {
                        const int components = std::popcount((unsigned int)(node.ComponentMask & 0x0f));
                        return components == 0 ? EGraphValueType::Float : TypeFromComponents(components);
                    }
                    case EMaterialNodeType::AppendVector:
                    {
                        const int components = Components(inputType(0)) + Components(inputType(1));
                        return components >= 2 && components <= 4
                            ? TypeFromComponents(components) : EGraphValueType::Float4;
                    }
                    case EMaterialNodeType::TexCoord:
                    case EMaterialNodeType::Panner: return EGraphValueType::Float2;
                    case EMaterialNodeType::Position: return EGraphValueType::Float3;
                    case EMaterialNodeType::Time: return EGraphValueType::Float;
                    case EMaterialNodeType::Sine:
                    case EMaterialNodeType::OneMinus:
                    case EMaterialNodeType::Saturate: return inputType(0);
                    case EMaterialNodeType::Multiply:
                    case EMaterialNodeType::Add:
                    case EMaterialNodeType::Subtract:
                    case EMaterialNodeType::Divide:
                    case EMaterialNodeType::Power:
                    case EMaterialNodeType::Lerp:
                    case EMaterialNodeType::StaticSwitch:
                        return Wider(inputType(0), inputType(1));
                    case EMaterialNodeType::FunctionInput:
                    case EMaterialNodeType::FunctionCall: return EGraphValueType::Float4;
                }
                return EGraphValueType::Float4;
            }

            const MaterialGraph& m_Graph;
            const std::unordered_map<uint32_t, SMaterialNode>& m_Nodes;
            SMaterialIR m_IR;
            std::unordered_map<uint32_t, SMaterialIRValue> m_Values;
            std::unordered_set<uint32_t> m_Visiting;
        };
    }

    SMaterialIR MaterialIRBuilder::Build(
        const MaterialGraph& graph, const EMaterialIRStage stage)
    {
        return SBuilder(graph, stage).Build();
    }
}
