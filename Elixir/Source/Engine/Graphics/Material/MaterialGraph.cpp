#include "epch.h"
#include "MaterialGraph.h"

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
            }
            return "float4";
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
            }
            return "BaseColor";
        }

        std::string Num(float v)
        {
            std::string s = std::to_string(v);
            return s;
        }

        // Coerce an expression of `from` type to the channel's expected type.
        std::string Coerce(const std::string& expr, EGraphValueType from, EMaterialChannel channel)
        {
            const bool scalarChannel = channel == EMaterialChannel::Metallic || channel == EMaterialChannel::Roughness;
            if (scalarChannel)
                return from == EGraphValueType::Float ? expr : "(" + expr + ").x";

            // float3 channel (BaseColor / Emissive / Normal).
            switch (from)
            {
                case EGraphValueType::Float:  return expr + ".xxx";
                case EGraphValueType::Float2: return "float3(" + expr + ", 0.0)";
                case EGraphValueType::Float3: return expr;
                case EGraphValueType::Float4: return "(" + expr + ").rgb";
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
            }
            return "0.0";
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

    std::string MaterialGraph::EmitNode(
        uint32_t id, std::unordered_map<uint32_t, std::string>& emitted, std::string& body) const
    {
        if (const auto it = emitted.find(id); it != emitted.end())
            return it->second;

        const auto nodeIt = m_Nodes.find(id);
        if (nodeIt == m_Nodes.end())
            return "0.0";
        const SMaterialNode& node = nodeIt->second;

        // Resolve each input to a variable name (recursing) or a default literal.
        std::vector<std::string> in;
        for (size_t i = 0; i < node.Inputs.size(); ++i)
        {
            if (node.Inputs[i] >= 0)
                in.push_back(EmitNode((uint32_t)node.Inputs[i], emitted, body));
            else if (i < node.DefaultInputs.size())
                in.push_back(node.DefaultInputs[i]);
            else
                in.push_back("0.0");
        }

        std::string expr;
        switch (node.Type)
        {
            case EMaterialNodeType::Constant:      expr = ConstantExpr(node); break;
            case EMaterialNodeType::Parameter:     expr = "mat." + node.ParameterName; break;
            case EMaterialNodeType::TextureSample: expr = node.TextureExpression; break;
            case EMaterialNodeType::TexCoord:      expr = "input.TexCoord"; break;
            case EMaterialNodeType::Multiply:      expr = "(" + in[0] + " * " + in[1] + ")"; break;
            case EMaterialNodeType::Add:           expr = "(" + in[0] + " + " + in[1] + ")"; break;
            case EMaterialNodeType::Subtract:      expr = "(" + in[0] + " - " + in[1] + ")"; break;
            case EMaterialNodeType::Divide:        expr = "(" + in[0] + " / " + in[1] + ")"; break;
            case EMaterialNodeType::Power:         expr = "pow(" + in[0] + ", " + in[1] + ")"; break;
            case EMaterialNodeType::Dot:           expr = "dot(" + in[0] + ", " + in[1] + ")"; break;
            case EMaterialNodeType::Lerp:          expr = "lerp(" + in[0] + ", " + in[1] + ", " + in[2] + ")"; break;
            case EMaterialNodeType::OneMinus:      expr = "(1.0 - " + in[0] + ")"; break;
            case EMaterialNodeType::Saturate:      expr = "saturate(" + in[0] + ")"; break;
            case EMaterialNodeType::Fresnel:       expr = "pow(saturate(1.0 - dot(N, V)), 5.0)"; break;
        }

        const std::string var = "n" + std::to_string(id);
        body += "    " + std::string(TypeName(node.OutputType)) + " " + var + " = " + expr + ";\n";
        emitted[id] = var;
        return var;
    }

    std::string MaterialGraph::GenerateHLSL() const
    {
        std::string body;
        std::unordered_map<uint32_t, std::string> emitted;

        for (const auto& [channel, nodeId] : m_Channels)
        {
            const std::string var = EmitNode(nodeId, emitted, body);
            const auto nodeIt = m_Nodes.find(nodeId);
            const EGraphValueType from = nodeIt != m_Nodes.end() ? nodeIt->second.OutputType : EGraphValueType::Float4;
            const auto ch = (EMaterialChannel)channel;
            body += "    surface." + std::string(ChannelName(ch)) + " = " + Coerce(var, from, ch) + ";\n";
        }
        return body;
    }
}
