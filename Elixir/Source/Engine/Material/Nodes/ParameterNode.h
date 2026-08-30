#pragma once

#include <Engine/Material/MaterialGraph.h>

namespace Elixir::MaterialNodes
{
    /** @brief Reads a named numeric material parameter. */
    class ParameterNode final : public IMaterialNode
    {
    public:
        /** @brief Creates a parameter reader with its expected value type. */
        ParameterNode(std::string name, const EMaterialValueType valueType)
            : m_Name(std::move(name)), m_ValueType(valueType) {}

        std::string_view GetTypeName() const override { return "material.parameter"; }
        const std::vector<SMaterialNodeInput>& GetInputs() const override { return m_Inputs; }

        bool Validate(const IMaterialNodeValidationContext& parameters, std::string& error) const override
        {
            if (parameters.HasValueParameter(m_Name, m_ValueType)) return true;
            error = "Invalid value parameter: " + m_Name;
            return false;
        }

        SMaterialExpression Emit(const SMaterialEmitContext& context) const override
        {
            return { context.ValueParameter(m_Name), m_ValueType };
        }

    private:
        std::string m_Name;
        EMaterialValueType m_ValueType;
        std::vector<SMaterialNodeInput> m_Inputs;
    };
}
