#include "epch.h"
#include "MaterialNode.h"

#include <Engine/Materials/Material.h>
#include <Engine/Materials/MaterialGraph.h>

namespace Elixir::Materials
{
    namespace
    {
        const SMaterialExpression s_ZeroExpression{
            .Code = "0.0",
            .ValueType = EMaterialValueType::Float
        };
    }

    const SMaterialExpression& MaterialEmitContext::Input(const uint32_t slot) const
    {
        return slot < m_Inputs.size() ? m_Inputs[slot] : s_ZeroExpression;
    }

    SMaterialExpression MaterialEmitContext::FunctionInput(
        const std::string& name,
        const EMaterialValueType type
    ) const
    {
        if (!m_Bindings)
            return { "0.0", type };

        const auto found = m_Bindings->FunctionInputs.find(name);
        if (found == m_Bindings->FunctionInputs.end())
            return { "0.0", type };

        return { Widen(found->second, type), type };
    }

    std::string MaterialEmitContext::ValueParameter(const std::string& name) const
    {
        if (m_Bindings && m_Bindings->Values.contains(name))
            return m_Bindings->Values.at(name);

        return "mat." + name;
    }

    std::string MaterialEmitContext::TextureParameter(const std::string& name) const
    {
        if (m_Bindings && m_Bindings->Textures.contains(name))
            return m_Bindings->Textures.at(name);

        return "mat." + name + ".x";
    }

    std::string MaterialEmitContext::Widen(
        const SMaterialExpression& expression,
        const EMaterialValueType type
    )
    {
        if (expression.ValueType == type) return expression.Code;

        if (expression.ValueType == EMaterialValueType::Float)
        {
            const char* swizzle = type == EMaterialValueType::Float2
                ? ".xx"
                : type == EMaterialValueType::Float3 ? ".xxx" : ".xxxx";
            return "(" + expression.Code + ")" + swizzle;
        }

        if (expression.ValueType == EMaterialValueType::Float2 &&
            type == EMaterialValueType::Float3)
            return "float3(" + expression.Code + ", 0.0)";

        if (expression.ValueType == EMaterialValueType::Float2 &&
            type == EMaterialValueType::Float4)
            return "float4(" + expression.Code + ", 0.0, 0.0)";

        if (expression.ValueType == EMaterialValueType::Float3 &&
            type == EMaterialValueType::Float4)
            return "float4(" + expression.Code + ", 1.0)";

        if (type == EMaterialValueType::Float)  return "(" + expression.Code + ").x";
        if (type == EMaterialValueType::Float2) return "(" + expression.Code + ").xy";
        if (type == EMaterialValueType::Float3) return "(" + expression.Code + ").xyz";

        return expression.Code;
    }

    const char* MaterialEmitContext::TypeName(const EMaterialValueType type)
    {
        switch (type)
        {
            case EMaterialValueType::Float:  return "float";
            case EMaterialValueType::Float2: return "float2";
            case EMaterialValueType::Float3: return "float3";
            case EMaterialValueType::Float4: return "float4";
        }

        return "float4";
    }

    int MaterialEmitContext::Components(const EMaterialValueType type)
    {
        switch (type)
        {
            case EMaterialValueType::Float:  return 1;
            case EMaterialValueType::Float2: return 2;
            case EMaterialValueType::Float3: return 3;
            case EMaterialValueType::Float4: return 4;
        }

        return 4;
    }

    EMaterialValueType MaterialEmitContext::Wider(
        const EMaterialValueType left,
        const EMaterialValueType right
    )
    {
        return Components(left) >= Components(right) ? left : right;
    }

    bool MaterialEmitContext::IsValidStage(const EShaderStage stage)
    {
        switch (stage)
        {
            case EShaderStage::Vertex:
            case EShaderStage::Hull:
            case EShaderStage::Domain:
            case EShaderStage::Geometry:
            case EShaderStage::Pixel:
            case EShaderStage::Compute:
                return true;
            default:
                return false;
        }
    }

    MaterialEmitContext::MaterialEmitContext(
        const std::vector<SMaterialExpression>& inputs,
        const SMaterialGraphBindings* bindings,
        const EShaderStage stage
    ) : m_Inputs(inputs),
        m_Bindings(bindings),
        m_Stage(stage) {
    }
}
