#pragma once

#include <Engine/Materials/MaterialFunction.h>

namespace Elixir::Materials::Nodes
{
    /** @brief Expand an immutable function using inputs from the calling graph. */
    class FunctionCall final : public MaterialNode
    {
      public:
        /**
         * @brief Retain a reusable function and expose its argument slots.
         * @param function Immutable function shared by callers.
         */
        explicit FunctionCall(Ref<const MaterialFunction> function)
          : m_Function(std::move(function))
        {
            if (!m_Function)
                return;

            // Default expressions are built without parameter or argument bindings.
            for (const auto& input : m_Function->GetInputs())
            {
                const auto value = input.DefaultValue;
                const auto type = input.ValueType;
                const auto components = MaterialEmitContext::Components(type);

                std::string expression = MaterialEmitContext::TypeName(type);
                expression += "(";

                for (int i = 0; i < components; ++i)
                {
                    if (i > 0) expression += ", ";
                    expression += std::to_string(value[i]);
                }

                expression += ")";

                m_Inputs.push_back({
                    input.Name,
                    type,
                    expression,
                    type
                });
            }
        }

        /** @copydoc MaterialNode::GetTypeName */
        std::string_view GetTypeName() const override { return "Material.FunctionCall"; }

        /** @copydoc MaterialNode::Validate */
        bool Validate(
            const MaterialNodeValidationContext& parameters,
            std::string& error
        ) const override
        {
            if (m_Function)
                return m_Function->Validate(parameters, error);

            error = "Material function is missing.";
            return false;
        }

        /** @copydoc MaterialNode::GetSupportedStages */
        EShaderStage GetSupportedStages(
            const SMaterialGraphBindings* bindings = nullptr
        ) const override
        {
            return m_Function
                ? m_Function->GetSupportedStages(bindings)
                : EShaderStage::None;
        }

        /** @copydoc MaterialNode::Emit */
        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            if (!m_Function)
                return { "0.0", EMaterialValueType::Float };

            return context.CallFunction(*m_Function);
        }

      private:
        Ref<const MaterialFunction> m_Function;
    };
}
