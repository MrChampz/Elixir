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
            }
            return 4;
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
        uint32_t id,
        bool vertexStage,
        std::unordered_map<uint32_t, std::string>& emitted,
        std::unordered_map<uint32_t, EGraphValueType>& types,
        std::string& body) const
    {
        if (const auto it = emitted.find(id); it != emitted.end())
            return it->second;

        const auto nodeIt = m_Nodes.find(id);
        if (nodeIt == m_Nodes.end())
        {
            types[id] = EGraphValueType::Float4;
            return "0.0";
        }
        const SMaterialNode& node = nodeIt->second;

        // Resolve each input to a variable name (recursing) or a default literal, and
        // remember the value type flowing out of each so ops can pick a common width.
        std::vector<std::string> in;
        std::vector<EGraphValueType> inT;
        for (size_t i = 0; i < node.Inputs.size(); ++i)
        {
            if (node.Inputs[i] >= 0)
            {
                in.push_back(EmitNode((uint32_t)node.Inputs[i], vertexStage, emitted, types, body));
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
            case EMaterialNodeType::ParamScalar:   expr = "GraphParams[" + std::to_string(node.ParamSlot) + "].x"; type = EGraphValueType::Float; break;
            case EMaterialNodeType::ParamColor:    expr = "GraphParams[" + std::to_string(node.ParamSlot) + "]"; type = EGraphValueType::Float4; break;
            case EMaterialNodeType::Parameter:     expr = "mat." + node.ParameterName; type = node.OutputType; break;
            case EMaterialNodeType::TexCoord:      expr = vertexStage ? "uv" : "input.TexCoord"; type = EGraphValueType::Float2; break;
            case EMaterialNodeType::Position:      expr = vertexStage ? "P" : "input.WorldPos"; type = EGraphValueType::Float3; break;
            case EMaterialNodeType::Time:          expr = "Time"; type = EGraphValueType::Float; break;
            case EMaterialNodeType::Sine:          expr = "sin(" + A(0) + ")"; type = AT(0); break;
            case EMaterialNodeType::TextureSample:
            {
                // node.TextureExpression holds the index accessor (e.g. mat.TexIndex0.x).
                const std::string idx = node.TextureExpression;
                const std::string uv = node.Inputs.empty() || node.Inputs[0] < 0
                    ? std::string("input.TexCoord") : Widen(A(0), AT(0), EGraphValueType::Float2);
                expr = "(" + idx + " == 0xffffffffu ? float3(1.0, 1.0, 1.0) : SampleTex(" + idx + ", " + uv + "))";
                type = EGraphValueType::Float3;
                break;
            }
            case EMaterialNodeType::Panner:
            {
                const std::string uv = node.Inputs.empty() || node.Inputs[0] < 0
                    ? std::string("input.TexCoord") : Widen(A(0), AT(0), EGraphValueType::Float2);
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
                    expr = "Checker(" + (has ? Widen(A(0), AT(0), EGraphValueType::Float2) : std::string("input.TexCoord")) + ", " + s + ")";
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
                    expr = "ValueNoise(" + (has ? Widen(A(0), AT(0), EGraphValueType::Float2) : std::string("input.TexCoord")) + ", " + s + ")";
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
            // Material-function nodes are expanded away in the editor before codegen;
            // these are safe fallbacks in case one is emitted directly.
            case EMaterialNodeType::FunctionInput:
            case EMaterialNodeType::FunctionCall:  expr = "float4(0.0, 0.0, 0.0, 1.0)"; type = EGraphValueType::Float4; break;
        }

        const std::string var = "n" + std::to_string(id);
        body += "    " + std::string(TypeName(type)) + " " + var + " = " + expr + ";\n";
        emitted[id] = var;
        types[id] = type;
        return var;
    }

    std::string MaterialGraph::GenerateHLSL(bool vertexStage) const
    {
        std::string body;
        std::unordered_map<uint32_t, std::string> emitted;
        std::unordered_map<uint32_t, EGraphValueType> types;

        for (const auto& [channel, nodeId] : m_Channels)
        {
            const auto ch = (EMaterialChannel)channel;
            const bool isWPO = ch == EMaterialChannel::WorldPositionOffset;
            // Pixel stage emits every surface channel except WPO; vertex stage emits
            // only WPO. This keeps each generated body limited to its stage's inputs.
            if (isWPO != vertexStage)
                continue;

            const std::string var = EmitNode(nodeId, vertexStage, emitted, types, body);
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
}
