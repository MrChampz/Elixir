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
                case EMaterialChannel::Normal:      return "Normal";
                case EMaterialChannel::Metallic:    return "Metallic";
                case EMaterialChannel::Roughness:   return "Roughness";
                case EMaterialChannel::Opacity:     return "Opacity";
                case EMaterialChannel::Emissive:    return "Emissive";
            }

            return "BaseColor";
        }

        std::string Num(const float value)
        {
            std::string str = std::to_string(value);
            return str;
        }

        int Components(EMaterialGraphValueType type)
        {
            switch (type)
            {
                case EMaterialGraphValueType::Float:    return 1;
                case EMaterialGraphValueType::Float2:   return 2;
                case EMaterialGraphValueType::Float3:   return 3;
                case EMaterialGraphValueType::Float4:   return 4;
            }

            return 4;
        }

        // The wider of two value types (more components wins). Use to pick a common
        // type for component-wise ops so mismatched pin widths still compile.
        EMaterialGraphValueType Wider(
            const EMaterialGraphValueType a,
            const EMaterialGraphValueType b
        )
        {
            return Components(a) >= Components(b) ? a : b;
        }

        // Coerce an expression from one value type to a wider (or equal) one: scalars
        // splat across all lanes; shorter vectors pad. Keeps generated HLSL well-typed
        // regardless of how the user wired the graph.
        std::string Widen(
            const std::string& expr,
            const EMaterialGraphValueType from,
            const EMaterialGraphValueType to
        )
        {
            if (from == to)
                return expr;

            if (from == EMaterialGraphValueType::Float)
            {
                const char* s = to == EMaterialGraphValueType::Float2
                    ? ".xx"
                    : to == EMaterialGraphValueType::Float3
                        ? ".xxx"
                        : ".xxxx";
                return "(" + expr + ")" + s;
            }

            if (from == EMaterialGraphValueType::Float2 && to == EMaterialGraphValueType::Float3)
                return "float3(" + expr + ", 0.0)";
            if (from == EMaterialGraphValueType::Float2 && to == EMaterialGraphValueType::Float4)
                return "float4(" + expr + ", 0.0, 0.0)";
            if (from == EMaterialGraphValueType::Float3 && to == EMaterialGraphValueType::Float4)
                return "float4(" + expr + ", 1.0)";

            // Narrowing (only if a wider value flows into a narrower slot): swizzle down.
            if (to == EMaterialGraphValueType::Float)  return "(" + expr + ").x";
            if (to == EMaterialGraphValueType::Float2) return "(" + expr + ").xy";
            if (to == EMaterialGraphValueType::Float3) return "(" + expr + ").xyz";

            return expr;
        }

        // Coerce an expression of 'from' type to the channel's expected type.
        std::string Coerce(
            const std::string& expr,
            const EMaterialGraphValueType from,
            const EMaterialChannel channel
        )
        {
            const bool scalarChannel = channel == EMaterialChannel::Metallic ||
                channel == EMaterialChannel::Roughness ||
                channel == EMaterialChannel::Opacity;

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
        return GenerateHLSL({});
    }

    std::string MaterialGraph::GenerateHLSL(const SMaterialGraphBindings& bindings) const
    {
        std::string body;
        std::unordered_map<uint32_t, std::string> emitted;
        std::unordered_map<uint32_t, EMaterialGraphValueType> types;

        for (const auto& [channelIndex, nodeId] : m_Channels)
        {
            const std::string var = EmitNode(nodeId, emitted, types, body, &bindings);
            const EMaterialGraphValueType from = types.contains(nodeId)
                ? types[nodeId]
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
        std::unordered_map<uint32_t, EMaterialGraphValueType>& types,
        std::string& body,
        const SMaterialGraphBindings* bindings
    ) const
    {
        if (const auto it = emitted.find(id); it != emitted.end())
            return it->second;

        const auto it = m_Nodes.find(id);
        if (it == m_Nodes.end())
        {
            types[id] = EMaterialGraphValueType::Float4;
            return "0.0";
        }

        const SMaterialNode& node = it->second;

        // Resolve each input to a variable name (recursing) or a default literal,
        // and remember the value type flowing out of each so ops can pick a common width.
        std::vector<std::string> in;
        std::vector<EMaterialGraphValueType> inTypes;
        for (size_t i = 0; i < node.Inputs.size(); ++i)
        {
            const auto& input = node.Inputs[i];

            if (input >= 0)
            {
                in.push_back(EmitNode((uint32_t)input, emitted, types, body, bindings));
                inTypes.push_back(types[(uint32_t)input]);
            }
            else if (i < node.DefaultInputs.size())
            {
                in.push_back(node.DefaultInputs[i]);
                inTypes.push_back(EMaterialGraphValueType::Float);
            }
            else
            {
                in.emplace_back("0.0");
                inTypes.push_back(EMaterialGraphValueType::Float);
            }
        }

        auto A = [&](size_t i) { return i < in.size() ? in[i] : std::string("0.0"); };
        auto AT = [&](size_t i) { return i < inTypes.size() ? inTypes[i] : EMaterialGraphValueType::Float; };

        std::string expr;
        EMaterialGraphValueType type = node.OutputType;

        // Component-wise binary op: coerce both operands to their common width.
        auto binOp = [&](const char* op)
        {
            const EMaterialGraphValueType to = Wider(AT(0), AT(1));
            expr = "(" + Widen(A(0), AT(0), to) + " " + op + " " + Widen(A(1), AT(1), to) + ")";
            type = to;
        };

        switch (node.Type)
        {
            case EMaterialNodeType::Constant:
                expr = ConstantExpr(node);
                type = node.OutputType;
                break;
            case EMaterialNodeType::Parameter:
                expr = bindings && bindings->Values.contains(node.ParameterName)
                    ? bindings->Values.at(node.ParameterName)
                    : "mat." + node.ParameterName;
                type = node.OutputType;
                break;
            case EMaterialNodeType::TexCoord:
                expr = "input.TexCoord";
                type = EMaterialGraphValueType::Float2;
                break;
            case EMaterialNodeType::TextureSample:
            {
                const std::string idx = bindings && bindings->Textures.contains(node.TextureParameterName)
                    ? bindings->Textures.at(node.TextureParameterName)
                    : "mat." + node.TextureParameterName + ".x";
                const std::string uv = node.Inputs.empty() || node.Inputs[0] < 0
                    ? "input.TexCoord"
                    : Widen(A(0), AT(0), EMaterialGraphValueType::Float2);
                expr = "(" + idx + " == 0xFFFFFFFFu ? float4(1.0, 1.0, 1.0, 1.0) : SampleTex(" + idx + ", " + uv + "))";
                type = EMaterialGraphValueType::Float4;
                break;
            }
            case EMaterialNodeType::ComponentMask:
            {
                static constexpr std::array components{ ".x", ".y", ".z", ".w" };
                const auto component = std::min(node.ComponentIndex, uint32_t(components.size() - 1));
                expr = "(" + A(0) + ")" + components[component];
                type = EMaterialGraphValueType::Float;
                break;
            }
            case EMaterialNodeType::Time:
                expr = "Time";
                type = EMaterialGraphValueType::Float;
                break;
            case EMaterialNodeType::Sine:
                expr = "sin(" + A(0) + ")";
                type = AT(0);
                break;
            case EMaterialNodeType::Panner:
            {
                const std::string uv = node.Inputs.empty() || node.Inputs[0] < 0
                    ? "input.TexCoord"
                    : Widen(A(0), AT(0), EMaterialGraphValueType::Float2);
                const std::string speed = "float2(" + Num(node.ConstantValue.x) + ", " + Num(node.ConstantValue.y) + ")";
                expr = "(" + uv + " + Time * " + speed + ")";
                type = EMaterialGraphValueType::Float2;
                break;
            }
            case EMaterialNodeType::Checkerboard:
            {
                const std::string uv = node.Inputs.empty() || node.Inputs[0] < 0
                    ? "input.TexCoord"
                    : Widen(A(0), AT(0), EMaterialGraphValueType::Float2);
                const std::string scale = Num(std::max(node.ConstantValue.x, 1.0f));
                expr = "(fmod(floor(" + uv + ".x * " + scale + ") + floor(" + uv + ".y * " + scale + "), 2.0) < 1.0 ? float3(0.08, 0.08, 0.08) : float3(0.72, 0.72, 0.72))";
                type = EMaterialGraphValueType::Float3;
                break;
            }
            case EMaterialNodeType::RadialGradientExponential:
            {
                const std::string uv = node.Inputs.empty() || node.Inputs[0] < 0
                    ? "input.TexCoord"
                    : Widen(A(0), AT(0), EMaterialGraphValueType::Float2);

                const std::string center = "float2(" + Num(node.RadialGradientCenter.x) + ", " + Num(node.RadialGradientCenter.y) + ")";
                const std::string radius = Num(std::max(node.RadialGradientRadius, 0.0001f));
                const std::string exponent = Num(std::max(node.RadialGradientExponent, 0.0001f));

                expr = "pow(saturate(1.0 - length((" + uv + " - " + center + ") / " + radius + ")), " + exponent + ")";
                type = EMaterialGraphValueType::Float;

                break;
            }
            case EMaterialNodeType::Multiply:
                binOp("*");
                break;
            case EMaterialNodeType::Add:
                binOp("+");
                break;
            case EMaterialNodeType::Subtract:
                binOp("-");
                break;
            case EMaterialNodeType::Divide:
                binOp("/");
                break;
            case EMaterialNodeType::Power:
            {
                const EMaterialGraphValueType to = Wider(AT(0), AT(1));
                expr = "pow(" + Widen(A(0), AT(0), to) + ", " + Widen(A(1), AT(1), to) + ")";
                type = to;
                break;
            }
            case EMaterialNodeType::Dot:
            {
                const EMaterialGraphValueType to = Wider(AT(0), AT(1));
                expr = "dot(" + Widen(A(0), AT(0), to) + ", " + Widen(A(1), AT(1), to) + ")";
                type = EMaterialGraphValueType::Float;
                break;
            }
            case EMaterialNodeType::Lerp:
            {
                const EMaterialGraphValueType to = Wider(AT(0), AT(1));
                expr = "lerp(" + Widen(A(0), AT(0), to) + ", " + Widen(A(1), AT(1), to)  + ", "
                    + Widen(A(2), AT(2), to) + ")";
                type = to;
                break;
            }
            case EMaterialNodeType::OneMinus:
                expr = "(1.0 - " + in[0] + ")";
                type = AT(0);
                break;
            case EMaterialNodeType::Saturate:
                expr = "saturate(" + in[0] + ")";
                type = AT(0);
                break;
            case EMaterialNodeType::Fresnel:
                expr = "pow(saturate(1.0 - dot(N, V)), 5.0)";
                type = EMaterialGraphValueType::Float;
                break;
        }

        const std::string var = "n" + std::to_string(id);
        body += "   " + std::string(TypeName(type)) + " " + var + " = " + expr + ";\n";
        emitted[id] = var;
        types[id] = type;
        return var;
    }
}
