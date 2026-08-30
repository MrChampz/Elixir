#pragma once

#include <Engine/Material/MaterialGraph.h>
#include <glm/glm.hpp>

namespace Elixir::MaterialNodes
{
    /** @brief Outputs a literal scalar or vector value. */
    class ConstantNode final : public IMaterialNode
    {
    public:
        /** @brief Creates a constant with a value and output type. */
        ConstantNode(const glm::vec4& value, const EMaterialValueType valueType)
            : m_Value(value), m_ValueType(valueType) {}

        std::string_view GetTypeName() const override { return "material.constant"; }
        const std::vector<SMaterialNodeInput>& GetInputs() const override { return m_Inputs; }

        SMaterialExpression Emit(const SMaterialEmitContext&) const override
        {
            const auto number = [](const float value) { return std::to_string(value); };
            switch (m_ValueType)
            {
                case EMaterialValueType::Float: return { number(m_Value.x), m_ValueType };
                case EMaterialValueType::Float2: return { "float2(" + number(m_Value.x) + ", " + number(m_Value.y) + ")", m_ValueType };
                case EMaterialValueType::Float3: return { "float3(" + number(m_Value.x) + ", " + number(m_Value.y) + ", " + number(m_Value.z) + ")", m_ValueType };
                case EMaterialValueType::Float4: return { "float4(" + number(m_Value.x) + ", " + number(m_Value.y) + ", " + number(m_Value.z) + ", " + number(m_Value.w) + ")", m_ValueType };
            }
            return { "0.0", EMaterialValueType::Float };
        }

    private:
        glm::vec4 m_Value;
        EMaterialValueType m_ValueType;
        std::vector<SMaterialNodeInput> m_Inputs;
    };
}
