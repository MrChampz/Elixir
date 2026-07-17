#include "epch.h"
#include "MaterialGraph.h"
#include "MaterialHLSLEmitter.h"
#include "MaterialIRBuilder.h"

#include <algorithm>
#include <bit>
#include <cctype>
#include <cmath>
#include <functional>
#include <unordered_set>

namespace Elixir
{
    namespace
    {
        const char* TypeName(EGraphValueType t)
        {
            switch (t)
            {
                case EGraphValueType::Float:  return "float";
                case EGraphValueType::Float2: return "float2";
                case EGraphValueType::Float3: return "float3";
                case EGraphValueType::Float4: return "float4";
                case EGraphValueType::Texture2D: return "uint"; // bindless descriptor index
            }
            return "float4";
        }

        // A texture parameter's slot carries its bindless index as a float value rather
        // than a bit pattern (see MaterialInstance::NO_TEXTURE_INDEX), so decode it by
        // converting, never by reinterpreting. The negative "no texture" marker wraps to
        // the same 0xffffffffu that Texture2D values already use for it.
        std::string TextureIndexFromSlot(const int32_t slot)
        {
            return "(uint)(int)GraphParams[" + std::to_string(slot) + "].x";
        }

        // The mesh UV, for a node whose UV input is left unconnected. The stages read it
        // from different places: the pixel body sees the interpolated `input`, while the
        // vertex body is injected into ComputeWPO(float3 P, float2 uv), where no `input`
        // exists. Naming it directly would compile only in the pixel stage.
        std::string DefaultUV(const bool vertexStage)
        {
            return vertexStage ? "uv" : "input.TexCoord";
        }

        const char* ChannelName(EMaterialChannel c)
        {
            switch (c)
            {
                case EMaterialChannel::BaseColor: return "BaseColor";
                case EMaterialChannel::Metallic:  return "Metallic";
                case EMaterialChannel::Roughness: return "Roughness";
                case EMaterialChannel::Emissive:  return "Emissive";
                case EMaterialChannel::Normal:    return "Normal";
                case EMaterialChannel::WorldPositionOffset: return "WorldPositionOffset";
                case EMaterialChannel::Opacity:   return "Opacity";
                case EMaterialChannel::SubsurfaceColor:    return "SubsurfaceColor";
                case EMaterialChannel::ClearCoat:          return "ClearCoat";
                case EMaterialChannel::ClearCoatRoughness: return "ClearCoatRoughness";
                case EMaterialChannel::Sheen:              return "Sheen";
            }
            return "BaseColor";
        }

        std::string Num(float v)
        {
            std::string s = std::to_string(v);
            return s;
        }

        int Components(EGraphValueType t)
        {
            switch (t)
            {
                case EGraphValueType::Float:  return 1;
                case EGraphValueType::Float2: return 2;
                case EGraphValueType::Float3: return 3;
                case EGraphValueType::Float4: return 4;
                case EGraphValueType::Texture2D: return 0;
            }
            return 4;
        }

        EGraphValueType TypeFromComponents(int components)
        {
            switch (components)
            {
                case 1: return EGraphValueType::Float;
                case 2: return EGraphValueType::Float2;
                case 3: return EGraphValueType::Float3;
                default: return EGraphValueType::Float4;
            }
        }

        // The wider of two value types (more components wins). Used to pick a common
        // type for component-wise ops so mismatched pin widths still compile.
        EGraphValueType Wider(EGraphValueType a, EGraphValueType b)
        {
            return Components(a) >= Components(b) ? a : b;
        }

        // Coerce an expression from one value type to a wider (or equal) one: scalars
        // splat across all lanes; shorter vectors pad. Keeps generated HLSL well-typed
        // regardless of how the user wired the graph.
        std::string Widen(const std::string& expr, EGraphValueType from, EGraphValueType to)
        {
            if (from == to)
                return expr;

            // Resource handles are not numeric graph values. Validate() reports an
            // incompatible connection; return a safe literal so speculative codegen
            // (used by the editor signature) still produces valid HLSL.
            if (from == EGraphValueType::Texture2D || to == EGraphValueType::Texture2D)
                return to == EGraphValueType::Texture2D ? "0xffffffffu" : "0.0";

            if (from == EGraphValueType::Float)
            {
                const char* s = to == EGraphValueType::Float2 ? ".xx" : to == EGraphValueType::Float3 ? ".xxx" : ".xxxx";
                return "(" + expr + ")" + s;
            }
            if (from == EGraphValueType::Float2 && to == EGraphValueType::Float3) return "float3(" + expr + ", 0.0)";
            if (from == EGraphValueType::Float2 && to == EGraphValueType::Float4) return "float4(" + expr + ", 0.0, 0.0)";
            if (from == EGraphValueType::Float3 && to == EGraphValueType::Float4) return "float4(" + expr + ", 1.0)";

            // Narrowing (only if a wider value flows into a narrower slot): swizzle down.
            if (to == EGraphValueType::Float)  return "(" + expr + ").x";
            if (to == EGraphValueType::Float2) return "(" + expr + ").xy";
            if (to == EGraphValueType::Float3) return "(" + expr + ").xyz";
            return expr;
        }

        // Coerce an expression of `from` type to the channel's expected type.
        std::string Coerce(const std::string& expr, EGraphValueType from, EMaterialChannel channel)
        {
            if (from == EGraphValueType::Texture2D)
                return "float3(0.0, 0.0, 0.0)";
            const bool scalarChannel = channel == EMaterialChannel::Metallic
                || channel == EMaterialChannel::Roughness || channel == EMaterialChannel::Opacity
                || channel == EMaterialChannel::ClearCoat || channel == EMaterialChannel::ClearCoatRoughness
                || channel == EMaterialChannel::Sheen;
            if (scalarChannel)
                return from == EGraphValueType::Float ? expr : "(" + expr + ").x";

            // float3 channel (BaseColor / Emissive / Normal).
            switch (from)
            {
                case EGraphValueType::Float:  return expr + ".xxx";
                case EGraphValueType::Float2: return "float3(" + expr + ", 0.0)";
                case EGraphValueType::Float3: return expr;
                case EGraphValueType::Float4: return "(" + expr + ").rgb";
                case EGraphValueType::Texture2D: break;
            }
            return expr;
        }

        std::string ConstantExpr(const SMaterialNode& node)
        {
            const glm::vec4& v = node.ConstantValue;
            switch (node.OutputType)
            {
                case EGraphValueType::Float:  return Num(v.x);
                case EGraphValueType::Float2: return "float2(" + Num(v.x) + ", " + Num(v.y) + ")";
                case EGraphValueType::Float3: return "float3(" + Num(v.x) + ", " + Num(v.y) + ", " + Num(v.z) + ")";
                case EGraphValueType::Float4:
                    return "float4(" + Num(v.x) + ", " + Num(v.y) + ", " + Num(v.z) + ", " + Num(v.w) + ")";
                case EGraphValueType::Texture2D: return "0xffffffffu";
            }
            return "0.0";
        }

        bool IsIdentifier(const std::string& value)
        {
            if (value.empty())
                return false;
            const auto first = static_cast<unsigned char>(value.front());
            if (std::isalpha(first) == 0 && value.front() != '_')
                return false;
            for (const char c : value)
            {
                const auto ch = static_cast<unsigned char>(c);
                if (std::isalnum(ch) == 0 && c != '_')
                    return false;
            }
            return true;
        }
    }

    uint32_t MaterialGraph::AddNode(const SMaterialNode& node)
    {
        SMaterialNode copy = node;
        copy.Id = m_NextId++;
        m_Nodes[copy.Id] = copy;
        return copy.Id;
    }

    void MaterialGraph::Connect(uint32_t fromNode, uint32_t toNode, uint32_t toSlot)
    {
        auto it = m_Nodes.find(toNode);
        if (it == m_Nodes.end())
            return;
        if (it->second.Inputs.size() <= toSlot)
            it->second.Inputs.resize(toSlot + 1, -1);
        it->second.Inputs[toSlot] = (int32_t)fromNode;
    }

    void MaterialGraph::SetChannel(EMaterialChannel channel, uint32_t nodeId)
    {
        m_Channels[(uint8_t)channel] = nodeId;
    }

    SMaterialGraphValidation MaterialGraph::Validate() const
    {
        SMaterialGraphValidation result;
        const auto sourceId = [&](uint32_t id)
        {
            const auto it = m_Nodes.find(id);
            return it != m_Nodes.end() && it->second.SourceId != 0 ? it->second.SourceId : id;
        };
        const auto add = [&](EMaterialDiagnosticSeverity severity, uint32_t nodeId, std::string message)
        {
            for (const auto& existing : result.Diagnostics)
                if (existing.Severity == severity && existing.NodeId == nodeId && existing.Message == message)
                    return;
            result.Diagnostics.push_back({ severity, nodeId, std::move(message) });
        };

        if ((uint8_t)m_BlendMode > (uint8_t)EMaterialBlendMode::Additive)
            add(EMaterialDiagnosticSeverity::Error, 0, "The graph has an invalid blend mode.");
        if ((uint8_t)m_ShadingModel > (uint8_t)EMaterialShadingModel::Cloth)
            add(EMaterialDiagnosticSeverity::Error, 0, "The graph has an invalid shading model.");
        if (!std::isfinite(m_AlphaCutoff) || m_AlphaCutoff < 0.0f || m_AlphaCutoff > 1.0f)
            add(EMaterialDiagnosticSeverity::Error, 0, "Alpha cutoff must be between zero and one.");

        const SMaterialDomainDescriptor* domain = MaterialDomain::Find(m_Domain);
        if (!domain)
            add(EMaterialDiagnosticSeverity::Error, 0, "The graph has an invalid material domain.");
        else
        {
            if (!domain->ShaderContractAvailable)
                add(EMaterialDiagnosticSeverity::Error, 0,
                    "Material domain '" + std::string(domain->Name)
                        + "' does not have a renderer shader contract yet.");

            const uint32_t usages = (uint32_t)m_Usages;
            if ((usages & ~MaterialDomain::KnownUsageMask()) != 0)
                add(EMaterialDiagnosticSeverity::Error, 0, "The graph contains unknown material usage flags.");
            if ((usages & ~(uint32_t)domain->AllowedUsages) != 0)
                add(EMaterialDiagnosticSeverity::Error, 0,
                    "The selected usages are incompatible with material domain '"
                        + std::string(domain->Name) + "'.");
            if (m_Domain == EMaterialDomain::Surface && usages == 0)
                add(EMaterialDiagnosticSeverity::Error, 0,
                    "Surface material must select at least one usage.");
        }

        std::unordered_set<uint32_t> reachable;
        const auto staticSwitchChoice = [&](const SMaterialNode& node)
        {
            if (node.Type != EMaterialNodeType::StaticSwitch || node.Inputs.size() < 3 || node.Inputs[2] < 0)
                return -1;
            const auto condition = m_Nodes.find((uint32_t)node.Inputs[2]);
            if (condition == m_Nodes.end()
                || (condition->second.Type != EMaterialNodeType::StaticBoolParameter
                    && condition->second.Type != EMaterialNodeType::Bool))
                return -1;
            return condition->second.ConstantValue.x >= 0.5f ? 0 : 1;
        };
        std::function<void(uint32_t)> markReachable = [&](uint32_t id)
        {
            if (!reachable.insert(id).second)
                return;
            const auto it = m_Nodes.find(id);
            if (it == m_Nodes.end())
                return;
            if (const int choice = staticSwitchChoice(it->second); choice >= 0)
            {
                if (it->second.Inputs[2] >= 0)
                    markReachable((uint32_t)it->second.Inputs[2]);
                if (it->second.Inputs[choice] >= 0)
                    markReachable((uint32_t)it->second.Inputs[choice]);
                return;
            }
            for (const int32_t input : it->second.Inputs)
                if (input >= 0)
                    markReachable((uint32_t)input);
        };

        if (m_Channels.empty())
            add(EMaterialDiagnosticSeverity::Warning, 0, "No material output is connected; shader defaults will be used.");
        for (const auto& [channel, nodeId] : m_Channels)
        {
            if (channel > (uint8_t)EMaterialChannel::Sheen)
            {
                add(EMaterialDiagnosticSeverity::Error, 0, "The graph contains an invalid material output channel.");
                continue;
            }
            if (domain && !domain->AllowsChannel(channel))
                add(EMaterialDiagnosticSeverity::Error, 0,
                    "Material output '" + std::string(ChannelName((EMaterialChannel)channel))
                        + "' is not available in domain '" + std::string(domain->Name) + "'.");
            if (!m_Nodes.contains(nodeId))
            {
                add(EMaterialDiagnosticSeverity::Error, 0,
                    "Material output '" + std::string(ChannelName((EMaterialChannel)channel)) + "' references a missing node.");
                continue;
            }
            if (m_Nodes.at(nodeId).OutputType == EGraphValueType::Texture2D)
                add(EMaterialDiagnosticSeverity::Error, sourceId(nodeId),
                    "Texture Object must be sampled before connecting it to a material output.");
            markReachable(nodeId);
        }

        std::unordered_map<std::string, std::pair<EGraphValueType, uint32_t>> parameters;
        for (const uint32_t id : reachable)
        {
            const auto it = m_Nodes.find(id);
            if (it == m_Nodes.end())
                continue;
            const SMaterialNode& node = it->second;
            const uint32_t diagnosticId = sourceId(id);

            if ((uint8_t)node.Type > (uint8_t)EMaterialNodeType::AppendVector)
            {
                add(EMaterialDiagnosticSeverity::Error, diagnosticId, "Node has an unknown type.");
                continue;
            }
            if ((uint8_t)node.OutputType > (uint8_t)EGraphValueType::Texture2D)
                add(EMaterialDiagnosticSeverity::Error, diagnosticId, "Node has an invalid output type.");

            for (size_t inputSlot = 0; inputSlot < node.Inputs.size(); ++inputSlot)
            {
                const int32_t input = node.Inputs[inputSlot];
                if (input >= 0 && !m_Nodes.contains((uint32_t)input))
                    add(EMaterialDiagnosticSeverity::Error, diagnosticId, "Node input references a missing node.");
                if (input < 0)
                    continue;
                const auto source = m_Nodes.find((uint32_t)input);
                if (source == m_Nodes.end() || source->second.OutputType != EGraphValueType::Texture2D)
                    continue;
                const bool acceptsTexture = (node.Type == EMaterialNodeType::TextureObjectSample && inputSlot == 0)
                    || node.Type == EMaterialNodeType::FunctionCall;
                if (!acceptsTexture)
                    add(EMaterialDiagnosticSeverity::Error, diagnosticId,
                        "Texture Object must be connected to a Sample Texture input.");
            }

            const glm::vec4& value = node.ConstantValue;
            if (!std::isfinite(value.x) || !std::isfinite(value.y)
                || !std::isfinite(value.z) || !std::isfinite(value.w))
                add(EMaterialDiagnosticSeverity::Error, diagnosticId, "Node contains a non-finite numeric value.");

            if (node.Type == EMaterialNodeType::Parameter && !IsIdentifier(node.ParameterName))
                add(EMaterialDiagnosticSeverity::Error, diagnosticId, "Material parameter must be a valid HLSL identifier.");
            if (node.Type == EMaterialNodeType::ParamScalar || node.Type == EMaterialNodeType::ParamColor
                || node.Type == EMaterialNodeType::TextureParameter
                || node.Type == EMaterialNodeType::TextureObjectParameter)
            {
                if (node.ParameterName.empty())
                    add(EMaterialDiagnosticSeverity::Error, diagnosticId, "Exposed parameter must have a name.");
                if (node.ParamSlot < 0 || node.ParamSlot >= 8)
                    add(EMaterialDiagnosticSeverity::Error, diagnosticId, "Exposed parameter exceeds the eight-slot graph parameter limit.");

                if (!node.ParameterName.empty())
                {
                    const EGraphValueType parameterType = node.Type == EMaterialNodeType::ParamScalar
                        ? EGraphValueType::Float
                        : node.Type == EMaterialNodeType::ParamColor
                            ? EGraphValueType::Float4 : EGraphValueType::Texture2D;
                    const auto [paramIt, inserted] = parameters.emplace(node.ParameterName,
                        std::pair{ parameterType, diagnosticId });
                    if (!inserted && paramIt->second.first != parameterType)
                    {
                        add(EMaterialDiagnosticSeverity::Warning, paramIt->second.second,
                            "Parameter '" + node.ParameterName + "' is declared with incompatible types.");
                        add(EMaterialDiagnosticSeverity::Warning, diagnosticId,
                            "Parameter '" + node.ParameterName + "' is declared with incompatible types.");
                    }
                }
            }
            if (node.Type == EMaterialNodeType::StaticBoolParameter && node.ParameterName.empty())
                add(EMaterialDiagnosticSeverity::Error, diagnosticId, "Static bool parameter must have a name.");
            if (node.Type == EMaterialNodeType::StaticSwitch)
            {
                if (node.Inputs.size() < 3 || node.Inputs[2] < 0)
                    add(EMaterialDiagnosticSeverity::Error, diagnosticId,
                        "Static Switch requires a Static Bool Parameter condition on input 3.");
                else
                {
                    const auto condition = m_Nodes.find((uint32_t)node.Inputs[2]);
                    if (condition == m_Nodes.end()
                        || (condition->second.Type != EMaterialNodeType::StaticBoolParameter
                            && condition->second.Type != EMaterialNodeType::Bool))
                        add(EMaterialDiagnosticSeverity::Error, diagnosticId,
                            "Static Switch condition must come from a Static Bool Parameter.");
                }
            }
            if (node.Type == EMaterialNodeType::TextureSample && node.TextureExpression.empty())
                add(EMaterialDiagnosticSeverity::Error, diagnosticId, "Texture sample has no texture binding.");
            if (node.Type == EMaterialNodeType::TextureObjectParameter
                && node.OutputType != EGraphValueType::Texture2D)
                add(EMaterialDiagnosticSeverity::Error, diagnosticId,
                    "Texture Object Parameter must output a Texture2D resource.");
            if (node.Type == EMaterialNodeType::TextureObjectSample)
            {
                if (node.Inputs.empty() || node.Inputs[0] < 0)
                    add(EMaterialDiagnosticSeverity::Error, diagnosticId,
                        "Sample Texture requires a Texture Object on input 1.");
                else if (const auto source = m_Nodes.find((uint32_t)node.Inputs[0]);
                    source != m_Nodes.end() && source->second.OutputType != EGraphValueType::Texture2D)
                    add(EMaterialDiagnosticSeverity::Error, diagnosticId,
                        "Sample Texture input 1 must be a Texture Object.");
                if ((uint8_t)node.TextureSampleType > (uint8_t)ETextureSampleType::Masks
                    || (uint8_t)node.TextureSampleAddress > (uint8_t)ETextureSampleAddress::Clamp
                    || (uint8_t)node.TextureSampleFilter > (uint8_t)ETextureSampleFilter::Point
                    || (uint8_t)node.TextureSampleMipMode > (uint8_t)ETextureSampleMipMode::Bias
                    || (uint8_t)node.TextureSampleOutput > (uint8_t)ETextureSampleOutput::RGBA)
                    add(EMaterialDiagnosticSeverity::Error, diagnosticId,
                        "Sample Texture contains an invalid sampling option.");
                const EGraphValueType expectedOutput = node.TextureSampleOutput == ETextureSampleOutput::RGBA
                    ? EGraphValueType::Float4
                    : node.TextureSampleOutput == ETextureSampleOutput::RGB
                        ? EGraphValueType::Float3 : EGraphValueType::Float;
                if (node.OutputType != expectedOutput)
                    add(EMaterialDiagnosticSeverity::Error, diagnosticId,
                        "Sample Texture output type does not match its selected channel.");
            }
            if (node.Type == EMaterialNodeType::ComponentMask)
            {
                const uint8_t mask = node.ComponentMask;
                if (mask == 0 || (mask & ~0x0fu) != 0)
                    add(EMaterialDiagnosticSeverity::Error, diagnosticId,
                        "Component Mask must select at least one valid RGBA channel.");
                else if (node.OutputType != TypeFromComponents(std::popcount((unsigned int)mask)))
                    add(EMaterialDiagnosticSeverity::Error, diagnosticId,
                        "Component Mask output type does not match its selected channels.");
            }
            if (node.Type == EMaterialNodeType::AppendVector)
            {
                const auto inputType = [&](size_t slot)
                {
                    if (slot >= node.Inputs.size() || node.Inputs[slot] < 0)
                        return EGraphValueType::Float;
                    const auto source = m_Nodes.find((uint32_t)node.Inputs[slot]);
                    return source == m_Nodes.end() ? EGraphValueType::Float : source->second.OutputType;
                };
                const int components = Components(inputType(0)) + Components(inputType(1));
                if (components > 4)
                    add(EMaterialDiagnosticSeverity::Error, diagnosticId,
                        "Append Vector inputs cannot contain more than four components in total.");
                else if (node.OutputType != TypeFromComponents(components))
                    add(EMaterialDiagnosticSeverity::Error, diagnosticId,
                        "Append Vector output type does not match its inputs.");
            }
            if (node.Type == EMaterialNodeType::Custom && node.CustomCode.empty())
                add(EMaterialDiagnosticSeverity::Warning, diagnosticId, "Custom HLSL is empty and will evaluate its first input.");
            if (node.Type == EMaterialNodeType::FunctionInput)
                add(EMaterialDiagnosticSeverity::Error, diagnosticId, "Function Input can only be used inside a material function.");
            if (node.Type == EMaterialNodeType::FunctionCall)
                add(EMaterialDiagnosticSeverity::Error, diagnosticId,
                    "Material function '" + node.ParameterName + "' could not be expanded; check the file, output and recursion.");
        }

        // Detect cycles only in nodes that contribute to an output. Disconnected work
        // in progress should not make an otherwise valid material uncompilable.
        std::unordered_map<uint32_t, uint8_t> visit;
        std::vector<uint32_t> stack;
        std::function<void(uint32_t)> visitNode = [&](uint32_t id)
        {
            visit[id] = 1;
            stack.push_back(id);
            const auto it = m_Nodes.find(id);
            if (it != m_Nodes.end())
            {
                for (const int32_t input : it->second.Inputs)
                {
                    if (input < 0 || !reachable.contains((uint32_t)input))
                        continue;
                    const uint32_t source = (uint32_t)input;
                    if (visit[source] == 0)
                        visitNode(source);
                    else if (visit[source] == 1)
                    {
                        const auto first = std::find(stack.begin(), stack.end(), source);
                        for (auto cycleNode = first; cycleNode != stack.end(); ++cycleNode)
                            add(EMaterialDiagnosticSeverity::Error, sourceId(*cycleNode), "Cycle detected in material graph.");
                    }
                }
            }
            stack.pop_back();
            visit[id] = 2;
        };
        for (const uint32_t id : reachable)
            if (visit[id] == 0)
                visitNode(id);

        // Fresnel depends on pixel-stage N/V and currently evaluates to zero in WPO.
        const auto wpo = m_Channels.find((uint8_t)EMaterialChannel::WorldPositionOffset);
        if (wpo != m_Channels.end() && m_Nodes.contains(wpo->second))
        {
            std::unordered_set<uint32_t> vertexNodes;
            std::function<void(uint32_t)> markVertex = [&](uint32_t id)
            {
                if (!vertexNodes.insert(id).second)
                    return;
                const auto it = m_Nodes.find(id);
                if (it == m_Nodes.end())
                    return;
                if (it->second.Type == EMaterialNodeType::Fresnel)
                    add(EMaterialDiagnosticSeverity::Error, sourceId(id), "Fresnel is not available in World Position Offset.");
                if (const int choice = staticSwitchChoice(it->second); choice >= 0)
                {
                    if (it->second.Inputs[choice] >= 0)
                        markVertex((uint32_t)it->second.Inputs[choice]);
                    return;
                }
                for (const int32_t input : it->second.Inputs)
                    if (input >= 0)
                        markVertex((uint32_t)input);
            };
            markVertex(wpo->second);
        }

        return result;
    }

    std::string MaterialGraph::EmitLegacyNode(
        uint32_t id,
        bool vertexStage,
        std::unordered_map<uint32_t, std::string>& emitted,
        std::unordered_map<uint32_t, EGraphValueType>& types,
        std::unordered_set<uint32_t>& visiting,
        std::string& body) const
    {
        if (const auto it = emitted.find(id); it != emitted.end())
            return it->second;
        if (!visiting.insert(id).second)
        {
            types[id] = EGraphValueType::Float;
            return "0.0"; // Validate() reports the cycle; keep direct codegen safe.
        }

        const auto nodeIt = m_Nodes.find(id);
        if (nodeIt == m_Nodes.end())
        {
            visiting.erase(id);
            types[id] = EGraphValueType::Float4;
            return "0.0";
        }
        const SMaterialNode& node = nodeIt->second;

        // A static switch aliases only the selected branch. The inactive branch is
        // never traversed or emitted, so it cannot add instructions or diagnostics to
        // this permutation.
        if (node.Type == EMaterialNodeType::StaticSwitch && node.Inputs.size() >= 3 && node.Inputs[2] >= 0)
        {
            const auto condition = m_Nodes.find((uint32_t)node.Inputs[2]);
            if (condition != m_Nodes.end()
                && (condition->second.Type == EMaterialNodeType::StaticBoolParameter
                    || condition->second.Type == EMaterialNodeType::Bool))
            {
                const size_t choice = condition->second.ConstantValue.x >= 0.5f ? 0u : 1u;
                const int32_t selected = node.Inputs[choice];
                const std::string expression = selected >= 0
                    ? EmitLegacyNode((uint32_t)selected, vertexStage, emitted, types, visiting, body)
                    : std::string("0.0");
                emitted[id] = expression;
                types[id] = selected >= 0 ? types[(uint32_t)selected] : EGraphValueType::Float;
                visiting.erase(id);
                return expression;
            }
        }

        // Resolve each input to a variable name (recursing) or a default literal, and
        // remember the value type flowing out of each so ops can pick a common width.
        std::vector<std::string> in;
        std::vector<EGraphValueType> inT;
        for (size_t i = 0; i < node.Inputs.size(); ++i)
        {
            if (node.Inputs[i] >= 0)
            {
                in.push_back(EmitLegacyNode((uint32_t)node.Inputs[i], vertexStage, emitted, types, visiting, body));
                inT.push_back(types[(uint32_t)node.Inputs[i]]);
            }
            else if (i < node.DefaultInputs.size())
            {
                in.push_back(node.DefaultInputs[i]);
                inT.push_back(EGraphValueType::Float);
            }
            else
            {
                in.push_back("0.0");
                inT.push_back(EGraphValueType::Float);
            }
        }

        auto A = [&](size_t i) { return i < in.size() ? in[i] : std::string("0.0"); };
        auto AT = [&](size_t i) { return i < inT.size() ? inT[i] : EGraphValueType::Float; };

        // Custom node: raw HLSL over inputs a, b, c, scoped in a block so multiple
        // Custom nodes don't collide. Emits its own multi-line body and returns early.
        if (node.Type == EMaterialNodeType::Custom)
        {
            const std::string var = "n" + std::to_string(id);
            body += "    " + std::string(TypeName(node.OutputType)) + " " + var + ";\n    {\n";
            const char* names[3] = { "a", "b", "c" };
            for (int i = 0; i < 3; ++i)
                body += "        " + std::string(TypeName(AT(i))) + " " + names[i] + " = " + A(i) + ";\n";
            const std::string code = node.CustomCode.empty() ? "a" : node.CustomCode;
            body += "        " + var + " = (" + code + ");\n    }\n";
            emitted[id] = var;
            types[id] = node.OutputType;
            visiting.erase(id);
            return var;
        }

        std::string expr;
        EGraphValueType type = node.OutputType;

        // Component-wise binary op: coerce both operands to their common width.
        auto binOp = [&](const char* op)
        {
            const EGraphValueType to = Wider(AT(0), AT(1));
            expr = "(" + Widen(A(0), AT(0), to) + " " + op + " " + Widen(A(1), AT(1), to) + ")";
            type = to;
        };

        switch (node.Type)
        {
            case EMaterialNodeType::Constant:      expr = ConstantExpr(node); type = node.OutputType; break;
            case EMaterialNodeType::Vector:        expr = ConstantExpr(node); type = node.OutputType; break;
            case EMaterialNodeType::Scalar:        expr = Num(node.ConstantValue.x); type = EGraphValueType::Float; break;
            case EMaterialNodeType::Bool:          expr = node.ConstantValue.x >= 0.5f ? "1.0" : "0.0"; type = EGraphValueType::Float; break;
            case EMaterialNodeType::StaticBoolParameter: expr = node.ConstantValue.x >= 0.5f ? "1.0" : "0.0"; type = EGraphValueType::Float; break;
            case EMaterialNodeType::ParamScalar:   expr = "GraphParams[" + std::to_string(node.ParamSlot) + "].x"; type = EGraphValueType::Float; break;
            case EMaterialNodeType::ParamColor:    expr = "GraphParams[" + std::to_string(node.ParamSlot) + "]"; type = EGraphValueType::Float4; break;
            case EMaterialNodeType::TextureParameter:
            {
                const std::string index = TextureIndexFromSlot(node.ParamSlot);
                const std::string uv = node.Inputs.empty() || node.Inputs[0] < 0
                    ? DefaultUV(vertexStage) : Widen(A(0), AT(0), EGraphValueType::Float2);
                expr = "(" + index + " == 0xffffffffu ? float3(1.0, 1.0, 1.0) : SampleTex("
                    + index + ", " + uv + "))";
                type = EGraphValueType::Float3;
                break;
            }
            case EMaterialNodeType::TextureObjectParameter:
                expr = TextureIndexFromSlot(node.ParamSlot);
                type = EGraphValueType::Texture2D;
                break;
            case EMaterialNodeType::TextureObjectSample:
            {
                const std::string index = !node.Inputs.empty() && node.Inputs[0] >= 0
                    ? A(0) : std::string("0xffffffffu");
                const std::string uv = node.Inputs.size() < 2 || node.Inputs[1] < 0
                    ? DefaultUV(vertexStage) : Widen(A(1), AT(1), EGraphValueType::Float2);
                const std::string mip = node.Inputs.size() < 3 || node.Inputs[2] < 0
                    ? Num(node.ConstantValue.x) : Widen(A(2), AT(2), EGraphValueType::Float);

                const bool point = node.TextureSampleFilter == ETextureSampleFilter::Point;
                const bool clamp = node.TextureSampleAddress == ETextureSampleAddress::Clamp;
                const std::string sampler = point
                    ? (clamp ? "texSamplerPointClamp" : "texSamplerPoint")
                    : (clamp ? "texSamplerClamp" : "texSampler");

                std::string sample;
                if (vertexStage || node.TextureSampleMipMode == ETextureSampleMipMode::Level)
                {
                    const std::string level = node.TextureSampleMipMode == ETextureSampleMipMode::Auto
                        ? std::string("0.0") : mip;
                    sample = "textures[" + index + "].SampleLevel(" + sampler + ", " + uv + ", " + level + ")";
                }
                else if (node.TextureSampleMipMode == ETextureSampleMipMode::Bias)
                    sample = "textures[" + index + "].SampleBias(" + sampler + ", " + uv + ", " + mip + ")";
                else
                    sample = "textures[" + index + "].Sample(" + sampler + ", " + uv + ")";

                // The texture format controls sRGB decoding. Color, Linear and Masks
                // therefore preserve the sampled value; Normal expands [0,1] to [-1,1].
                const bool normal = node.TextureSampleType == ETextureSampleType::Normal;
                const std::string fallback = normal
                    ? std::string("float4(0.5, 0.5, 1.0, 1.0)")
                    : std::string("float4(1.0, 1.0, 1.0, 1.0)");
                const std::string raw = "(" + index + " == 0xffffffffu ? " + fallback + " : " + sample + ")";

                if (node.TextureSampleOutput == ETextureSampleOutput::RGBA)
                {
                    expr = raw;
                    if (normal)
                        expr = "(" + expr + " * float4(2.0, 2.0, 2.0, 1.0)"
                            " - float4(1.0, 1.0, 1.0, 0.0))";
                    type = EGraphValueType::Float4;
                }
                else if (node.TextureSampleOutput == ETextureSampleOutput::RGB)
                {
                    expr = "(" + raw + ").rgb";
                    if (normal)
                        expr = "(" + expr + " * 2.0 - 1.0)";
                    type = EGraphValueType::Float3;
                }
                else
                {
                    const char channel = node.TextureSampleOutput == ETextureSampleOutput::R ? 'r'
                        : node.TextureSampleOutput == ETextureSampleOutput::G ? 'g'
                        : node.TextureSampleOutput == ETextureSampleOutput::B ? 'b' : 'a';
                    expr = "(" + raw + ")." + std::string(1, channel);
                    if (normal && channel != 'a')
                        expr = "(" + expr + " * 2.0 - 1.0)";
                    type = EGraphValueType::Float;
                }
                break;
            }
            case EMaterialNodeType::ComponentMask:
            {
                const uint8_t mask = node.ComponentMask & 0x0f;
                std::string swizzle;
                if (mask & 0x1) swizzle += 'r';
                if (mask & 0x2) swizzle += 'g';
                if (mask & 0x4) swizzle += 'b';
                if (mask & 0x8) swizzle += 'a';
                const int components = std::popcount((unsigned int)mask);
                if (components == 0)
                {
                    expr = "0.0";
                    type = EGraphValueType::Float;
                }
                else
                {
                    const std::string value = Widen(A(0), AT(0), EGraphValueType::Float4);
                    expr = "(" + value + ")." + swizzle;
                    type = TypeFromComponents(components);
                }
                break;
            }
            case EMaterialNodeType::AppendVector:
            {
                const int components = Components(AT(0)) + Components(AT(1));
                if (components < 2 || components > 4)
                {
                    expr = "float4(0.0, 0.0, 0.0, 0.0)";
                    type = EGraphValueType::Float4;
                }
                else
                {
                    type = TypeFromComponents(components);
                    expr = std::string(TypeName(type)) + "(" + A(0) + ", " + A(1) + ")";
                }
                break;
            }
            case EMaterialNodeType::Parameter:     expr = "mat." + node.ParameterName; type = node.OutputType; break;
            case EMaterialNodeType::TexCoord:      expr = DefaultUV(vertexStage); type = EGraphValueType::Float2; break;
            case EMaterialNodeType::Position:      expr = vertexStage ? "P" : "input.WorldPos"; type = EGraphValueType::Float3; break;
            case EMaterialNodeType::Time:          expr = "Time"; type = EGraphValueType::Float; break;
            case EMaterialNodeType::Sine:          expr = "sin(" + A(0) + ")"; type = AT(0); break;
            case EMaterialNodeType::TextureSample:
            {
                // node.TextureExpression holds the index accessor (e.g. mat.TexIndex0.x).
                const std::string idx = node.TextureExpression;
                const std::string uv = node.Inputs.empty() || node.Inputs[0] < 0
                    ? DefaultUV(vertexStage) : Widen(A(0), AT(0), EGraphValueType::Float2);
                expr = "(" + idx + " == 0xffffffffu ? float3(1.0, 1.0, 1.0) : SampleTex(" + idx + ", " + uv + "))";
                type = EGraphValueType::Float3;
                break;
            }
            case EMaterialNodeType::Panner:
            {
                const std::string uv = node.Inputs.empty() || node.Inputs[0] < 0
                    ? DefaultUV(vertexStage) : Widen(A(0), AT(0), EGraphValueType::Float2);
                const std::string speed = "float2(" + Num(node.ConstantValue.x) + ", " + Num(node.ConstantValue.y) + ")";
                expr = "(" + uv + " + Time * " + speed + ")";
                type = EGraphValueType::Float2;
                break;
            }
            case EMaterialNodeType::Checker:
            {
                const bool has = !node.Inputs.empty() && node.Inputs[0] >= 0;
                const std::string s = Num(node.ConstantValue.x);
                if (has && AT(0) == EGraphValueType::Float3)
                    expr = "Checker3(" + A(0) + ", " + s + ")"; // world-space, UV-independent
                else
                    expr = "Checker(" + (has ? Widen(A(0), AT(0), EGraphValueType::Float2) : DefaultUV(vertexStage)) + ", " + s + ")";
                type = EGraphValueType::Float;
                break;
            }
            case EMaterialNodeType::Noise:
            {
                const bool has = !node.Inputs.empty() && node.Inputs[0] >= 0;
                const std::string s = Num(node.ConstantValue.x);
                if (has && AT(0) == EGraphValueType::Float3)
                    expr = "ValueNoise3(" + A(0) + ", " + s + ")"; // world-space, UV-independent
                else
                    expr = "ValueNoise(" + (has ? Widen(A(0), AT(0), EGraphValueType::Float2) : DefaultUV(vertexStage)) + ", " + s + ")";
                type = EGraphValueType::Float;
                break;
            }
            case EMaterialNodeType::Multiply:      binOp("*"); break;
            case EMaterialNodeType::Add:           binOp("+"); break;
            case EMaterialNodeType::Subtract:      binOp("-"); break;
            case EMaterialNodeType::Divide:        binOp("/"); break;
            case EMaterialNodeType::Power:
            {
                const EGraphValueType to = Wider(AT(0), AT(1));
                expr = "pow(" + Widen(A(0), AT(0), to) + ", " + Widen(A(1), AT(1), to) + ")";
                type = to;
                break;
            }
            case EMaterialNodeType::Dot:
            {
                const EGraphValueType to = Wider(AT(0), AT(1));
                expr = "dot(" + Widen(A(0), AT(0), to) + ", " + Widen(A(1), AT(1), to) + ")";
                type = EGraphValueType::Float;
                break;
            }
            case EMaterialNodeType::Lerp:
            {
                const EGraphValueType to = Wider(AT(0), AT(1));
                expr = "lerp(" + Widen(A(0), AT(0), to) + ", " + Widen(A(1), AT(1), to) + ", "
                     + Widen(A(2), AT(2), to) + ")";
                type = to;
                break;
            }
            case EMaterialNodeType::OneMinus:      expr = "(1.0 - " + A(0) + ")"; type = AT(0); break;
            case EMaterialNodeType::Saturate:      expr = "saturate(" + A(0) + ")"; type = AT(0); break;
            case EMaterialNodeType::Fresnel:       expr = vertexStage ? "0.0" : "pow(saturate(1.0 - dot(N, V)), 5.0)"; type = EGraphValueType::Float; break;
            case EMaterialNodeType::StaticSwitch:
            {
                const EGraphValueType to = Wider(AT(0), AT(1));
                expr = "(" + A(2) + " >= 0.5 ? " + Widen(A(0), AT(0), to)
                    + " : " + Widen(A(1), AT(1), to) + ")";
                type = to;
                break;
            }
            case EMaterialNodeType::Custom:         break; // handled above (multi-line body)
            // Material-function nodes are expanded away in the editor before codegen;
            // these are safe fallbacks in case one is emitted directly.
            case EMaterialNodeType::FunctionInput:
            case EMaterialNodeType::FunctionCall:  expr = "float4(0.0, 0.0, 0.0, 1.0)"; type = EGraphValueType::Float4; break;
        }

        const std::string var = "n" + std::to_string(id);
        body += "    " + std::string(TypeName(type)) + " " + var + " = " + expr + ";\n";
        emitted[id] = var;
        types[id] = type;
        visiting.erase(id);
        return var;
    }

    std::string MaterialGraph::GenerateLegacyHLSL(bool vertexStage) const
    {
        std::string body;
        std::unordered_map<uint32_t, std::string> emitted;
        std::unordered_map<uint32_t, EGraphValueType> types;
        std::unordered_set<uint32_t> visiting;

        for (uint8_t channel = 0; channel <= (uint8_t)EMaterialChannel::Sheen; ++channel)
        {
            const auto output = m_Channels.find(channel);
            if (output == m_Channels.end())
                continue;
            const uint32_t nodeId = output->second;
            const auto ch = (EMaterialChannel)channel;
            const bool isWPO = ch == EMaterialChannel::WorldPositionOffset;
            // Pixel stage emits every surface channel except WPO; vertex stage emits
            // only WPO. This keeps each generated body limited to its stage's inputs.
            if (isWPO != vertexStage)
                continue;

            const std::string var = EmitLegacyNode(nodeId, vertexStage, emitted, types, visiting, body);
            const EGraphValueType from = types.count(nodeId) ? types[nodeId] : EGraphValueType::Float4;
            if (isWPO)
                body += "    wpo = " + Coerce(var, from, ch) + ";\n";
            else
                body += "    surface." + std::string(ChannelName(ch)) + " = " + Coerce(var, from, ch) + ";\n";
        }

        // Masked: alpha-test against the cutoff (pixel stage only).
        if (!vertexStage && m_BlendMode == EMaterialBlendMode::Masked)
            body += "    clip(surface.Opacity - " + Num(m_AlphaCutoff) + ");\n";

        return body;
    }

    std::string MaterialGraph::GenerateHLSL(const bool vertexStage) const
    {
        const EMaterialIRStage stage = vertexStage
            ? EMaterialIRStage::Vertex : EMaterialIRStage::Pixel;
        return MaterialHLSLEmitter::Emit(MaterialIRBuilder::Build(*this, stage));
    }
}
