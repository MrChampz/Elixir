#pragma once

#include <Engine/Materials/Material.h>

namespace Elixir::Materials::Nodes
{
    /** @brief Read one typed argument of a reusable material function. */
    class FunctionInput final : public MaterialNode
    {
    public:
        /**
         * @brief Read an argument by name.
         * @param name Argument name.
         * @param type Declared numeric type.
         */
        FunctionInput(std::string name, const EMaterialValueType type)
          : m_Name(std::move(name)),
            m_Type(type) {}

        /** @copydoc MaterialNode::GetTypeName */
        std::string_view GetTypeName() const override { return "Material.FunctionInput"; }

        /** @copydoc MaterialNode::Validate */
        bool Validate(
            const MaterialNodeValidationContext& parameters,
            std::string& error
        ) const override
        {
            if (parameters.HasFunctionInput(m_Name, m_Type))
                return true;

            error = "Unknown or incompatible function argument: " + m_Name;
            return false;
        }

        /** @copydoc MaterialNode::Emit */
        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            return context.FunctionInput(m_Name, m_Type);
        }

    private:
        std::string m_Name;
        EMaterialValueType m_Type;
    };
}
