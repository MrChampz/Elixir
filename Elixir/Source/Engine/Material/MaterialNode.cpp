#include "epch.h"
#include "MaterialNode.h"

#include <Engine/Material/MaterialGraph.h>

namespace Elixir
{
    namespace
    {
        const SMaterialExpression s_ZeroExpression{ "0.0", EMaterialValueType::Float };
    }

    SMaterialEmitContext::SMaterialEmitContext(
        const std::vector<SMaterialExpression>& inputs,
        const SMaterialGraphBindings* bindings
    )
        : m_Inputs(inputs), m_Bindings(bindings)
    {
    }

    const SMaterialExpression& SMaterialEmitContext::Input(const uint32_t slot) const
    {
        return slot < m_Inputs.size() ? m_Inputs[slot] : s_ZeroExpression;
    }

    std::string SMaterialEmitContext::ValueParameter(const std::string_view name) const
    {
        if (m_Bindings && m_Bindings->Values.contains(std::string(name)))
            return m_Bindings->Values.at(std::string(name));

        return "mat." + std::string(name);
    }

    std::string SMaterialEmitContext::TextureParameter(const std::string_view name) const
    {
        if (m_Bindings && m_Bindings->Textures.contains(std::string(name)))
            return m_Bindings->Textures.at(std::string(name));

        return "mat." + std::string(name) + ".x";
    }

    std::string SMaterialEmitContext::Widen(
        const SMaterialExpression& expression,
        const EMaterialValueType type
    ) const
    {
        if (expression.ValueType == type)
            return expression.Code;

        if (expression.ValueType == EMaterialValueType::Float)
        {
            const char* swizzle = type == EMaterialValueType::Float2
                ? ".xx"
                : type == EMaterialValueType::Float3 ? ".xxx" : ".xxxx";
            return "(" + expression.Code + ")" + swizzle;
        }

        if (expression.ValueType == EMaterialValueType::Float2 && type == EMaterialValueType::Float3)
            return "float3(" + expression.Code + ", 0.0)";
        if (expression.ValueType == EMaterialValueType::Float2 && type == EMaterialValueType::Float4)
            return "float4(" + expression.Code + ", 0.0, 0.0)";
        if (expression.ValueType == EMaterialValueType::Float3 && type == EMaterialValueType::Float4)
            return "float4(" + expression.Code + ", 1.0)";

        if (type == EMaterialValueType::Float) return "(" + expression.Code + ").x";
        if (type == EMaterialValueType::Float2) return "(" + expression.Code + ").xy";
        if (type == EMaterialValueType::Float3) return "(" + expression.Code + ").xyz";
        return expression.Code;
    }

    const char* SMaterialEmitContext::TypeName(const EMaterialValueType type)
    {
        switch (type)
        {
            case EMaterialValueType::Float: return "float";
            case EMaterialValueType::Float2: return "float2";
            case EMaterialValueType::Float3: return "float3";
            case EMaterialValueType::Float4: return "float4";
        }

        return "float4";
    }

    int SMaterialEmitContext::Components(const EMaterialValueType type)
    {
        switch (type)
        {
            case EMaterialValueType::Float: return 1;
            case EMaterialValueType::Float2: return 2;
            case EMaterialValueType::Float3: return 3;
            case EMaterialValueType::Float4: return 4;
        }

        return 4;
    }

    EMaterialValueType SMaterialEmitContext::Wider(
        const EMaterialValueType left,
        const EMaterialValueType right
    )
    {
        return Components(left) >= Components(right) ? left : right;
    }

    bool IMaterialNode::Validate(const IMaterialNodeValidationContext&, std::string&) const
    {
        return true;
    }
}
