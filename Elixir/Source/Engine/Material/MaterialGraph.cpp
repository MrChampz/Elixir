#include "epch.h"
#include "MaterialGraph.h"

namespace Elixir
{
    namespace
    {
        const char* TypeName(const EMaterialGraphValueType type)
        {
            switch (type)
            {
                case EMaterialGraphValueType::Float:    return "float";
                case EMaterialGraphValueType::Float2:   return "float2";
                case EMaterialGraphValueType::Float3:   return "float3";
                case EMaterialGraphValueType::Float4:   return "float4";
            }

            return "float4";
        }

        const char* ChannelName(const EMaterialChannel channel)
        {
            switch (channel)
            {
                case EMaterialChannel::BaseColor:   return "BaseColor";
                case EMaterialChannel::Metallic:    return "Metallic";
                case EMaterialChannel::Roughness:   return "Roughness";
                case EMaterialChannel::Emissive:    return "Emissive";
                case EMaterialChannel::Normal:      return "Normal";
            }

            return "BaseColor";
        }

        std::string Num(const float value)
        {
            std::string str = std::to_string(value);
            return str;
        }

        // Coerce an expression of 'from' type to the channel's expected type.
        std::string Coerce(
            const std::string& expr,
            const EMaterialGraphValueType from,
            const EMaterialChannel channel
        )
        {
            const bool scalarChannel = channel == EMaterialChannel::Metallic ||
                channel == EMaterialChannel::Roughness;

            if (scalarChannel)
                return from == EMaterialGraphValueType::Float ? expr : "(" + expr + ").x";

            // float3 channel (BaseColor/Emissive/Normal).
            switch (from)
            {
                case EMaterialGraphValueType::Float:    return expr + ".xxx";
                case EMaterialGraphValueType::Float2:   return "float3(" + expr + ", 0.0)";
                case EMaterialGraphValueType::Float3:   return expr;
                case EMaterialGraphValueType::Float4:   return "(" + expr + ").rgb";
            }

            return expr;
        }

        std::string ConstantExpr(const SMaterialNode& node)
        {
            const glm::vec4& v = node.ConstantValue;

            switch (node.OutputType)
            {
                case EMaterialGraphValueType::Float:
                    return Num(v.x);
                case EMaterialGraphValueType::Float2:
                    return "float2(" + Num(v.x) + ", " + Num(v.y) + ")";
                case EMaterialGraphValueType::Float3:
                    return "float3(" + Num(v.x) + ", " + Num(v.y) + ", " + Num(v.z) + ")";
                case EMaterialGraphValueType::Float4:
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

    void MaterialGraph::Connect(
        const uint32_t fromNode,
        const uint32_t toNode,
        const uint32_t toSlot
    )
    {
        const auto it = m_Nodes.find(toNode);
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

    std::string MaterialGraph::GenerateHLSL() const
    {
        std::string body;
        std::unordered_map<uint32_t, std::string> emitted;

        for (const auto& [channelIndex, nodeId] : m_Channels)
        {
            const std::string var = EmitNode(nodeId, emitted, body);
            const auto it = m_Nodes.find(nodeId);

            const EMaterialGraphValueType from = it != m_Nodes.end()
                ? it->second.OutputType
                : EMaterialGraphValueType::Float4;

            const auto channel = (EMaterialChannel)channelIndex;
            const auto channelName = ChannelName(channel);
            body += "   surface." + std::string(channelName) + " = " + Coerce(var, from, channel) + ";\n";
        }

        return body;
    }

    std::string MaterialGraph::EmitNode(
        const uint32_t id,
        std::unordered_map<uint32_t, std::string>& emitted,
        std::string& body
    ) const
    {
        if (const auto it = emitted.find(id); it != emitted.end())
            return it->second;

        const auto it = m_Nodes.find(id);
        if (it == m_Nodes.end())
            return "0.0";

        const SMaterialNode& node = it->second;

        // Resolve each input to a variable name(recursing) or a default literal.
        std::vector<std::string> in;
        for (size_t i = 0; i < node.Inputs.size(); ++i)
        {
            const auto& input = node.Inputs[i];

            if (input >= 0)
                in.push_back(EmitNode((uint32_t)input, emitted, body));
            else if (i < node.DefaultInputs.size())
                in.push_back(node.DefaultInputs[i]);
            else
                in.emplace_back("0.0");
        }

        std::string expr;
        switch (node.Type)
        {
            case EMaterialNodeType::Constant:
                expr = ConstantExpr(node);
                break;
            case EMaterialNodeType::Parameter:
                expr = "mat." + node.ParameterName;
                break;
            case EMaterialNodeType::TextureSample:
                expr = node.TextureExpression;
                break;
            case EMaterialNodeType::Multiply:
                expr = "(" + in[0] + " * " + in[1] + ")";
                break;
            case EMaterialNodeType::Add:
                expr = "(" + in[0] + " + " + in[1] + ")";
                break;
            case EMaterialNodeType::Lerp:
                expr = "lerp(" + in[0] + ", " + in[1] + ", " + in[2] + ")";
                break;
            case EMaterialNodeType::Fresnel:
                expr = "pow(saturate(1.0 - dot(N, V)), 5.0)";
                break;
        }

        const std::string var = "n" + std::to_string(id);
        body += "   " + std::string(TypeName(node.OutputType)) + " " + var + " = " + expr + ";\n";
        emitted[id] = var;
        return var;
    }
}
