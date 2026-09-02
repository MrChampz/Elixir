#pragma once

#include <Engine/Materials/Material.h>
#include <Engine/Materials/MaterialNode.h>

namespace Elixir::Materials::Nodes
{
    /**
     * @brief Outputs a literal scalar or vector value.
     */
    class Constant final : public MaterialNode
    {
    public:
        /**
         * @brief Creates a constant with a value and output type.
         * @param value The constant value.
         * @param type The output type.
         */
        Constant(const glm::vec4& value, const EMaterialValueType type)
          : m_Value(value),
            m_ValueType(type) {}

        std::string_view GetTypeName() const override { return "Material.Constant"; }

        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            const auto number = [](const float value) { return std::to_string(value); };

            switch (m_ValueType)
            {
                case EMaterialValueType::Float: return { number(m_Value.x), m_ValueType };
                case EMaterialValueType::Float2:
                    return {
                        "float2(" + number(m_Value.x) + ", " + number(m_Value.y) + ")",
                        m_ValueType
                    };
                case EMaterialValueType::Float3:
                    return {
                        "float3(" + number(m_Value.x) + ", " + number(m_Value.y) + ", " +
                            number(m_Value.z) + ")",
                        m_ValueType
                    };
                case EMaterialValueType::Float4:
                    return {
                        "float4(" + number(m_Value.x) + ", " + number(m_Value.y) + ", " +
                            number(m_Value.z) + ", " + number(m_Value.w) + ")",
                        m_ValueType
                    };
            }

            return { "0.0", EMaterialValueType::Float };
        }

    private:
        glm::vec4 m_Value;
        EMaterialValueType m_ValueType;
    };
}