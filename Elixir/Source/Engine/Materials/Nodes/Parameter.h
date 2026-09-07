#pragma once

#include <Engine/Materials/Material.h>
#include <Engine/Materials/MaterialNode.h>

namespace Elixir::Materials::Nodes
{
    /**
     * @brief Reads a named numeric material parameter.
     */
    class Parameter final : public MaterialNode
    {
    public:
        /**
         * @brief Creates a parameter reader with its expected value type.
         * @param name Parameter name.
         * @param type Expected value type.
         */
        Parameter(std::string name, const EMaterialValueType type)
          : m_Name(std::move(name)),
            m_ValueType(type) {}

        std::string_view GetTypeName() const override { return "Material.Parameter"; }

        bool Validate(
            const MaterialNodeValidationContext& parameters,
            std::string& error
        ) const override
        {
            if (parameters.HasValueParameter(m_Name, m_ValueType)) return true;
            error = "Invalid value parameter: " + m_Name;
            return false;
        }

        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            return {
                .Code = context.ValueParameter(m_Name),
                .ValueType =  m_ValueType
            };
        }

    private:
        std::string m_Name;
        EMaterialValueType m_ValueType;
    };
}