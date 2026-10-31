#pragma once

#include <Engine/Materials/Material.h>

namespace Elixir::Materials::Nodes
{
    /**
     * @brief Emit an authored HLSL expression with positional input placeholders.
     * Use {0}, {1}, and so on for inputs. Shader compilation validates the HLSL.
     */
    class Custom final : public MaterialNode
    {
    public:
        /**
         * @brief Create a custom expression.
         * @param expression HLSL expression containing positional placeholders.
         * @param type Numeric output type.
         * @param inputs Typed input slots and defaults.
         * @param supportedStages Stages in which the authored HLSL expression is valid.
         */
        Custom(
            std::string expression,
            const EMaterialValueType type,
            std::vector<SMaterialNodeInput> inputs = {},
            const EShaderStage supportedStages = EShaderStage::Pixel
        ) : MaterialNode(std::move(inputs)),
            m_Expression(std::move(expression)),
            m_Type(type),
            m_SupportedStages(supportedStages) {}

        /** @copydoc MaterialNode::GetTypeName */
        std::string_view GetTypeName() const override { return "Material.Custom"; }

        /** @copydoc MaterialNode::GetSupportedStages */
        EShaderStage GetSupportedStages(
            const SMaterialGraphBindings* bindings = nullptr
        ) const override
        {
            return m_SupportedStages;
        }

        /** @copydoc MaterialNode::Validate */
        bool Validate(
            const MaterialNodeValidationContext& parameters,
            std::string& error
        ) const override
        {
            if (!m_Expression.empty())
                return true;

            error = "Custom HLSL expression is empty.";
            return false;
        }

        /** @copydoc MaterialNode::Emit */
        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            std::string result;
            for (size_t i = 0; i < m_Expression.size();)
            {
                bool replaced = false;

                for (uint32_t slot = 0; slot < m_Inputs.size(); ++slot)
                {
                    const auto token = "{" + std::to_string(slot) + "}";

                    if (m_Expression.compare(i, token.size(), token) != 0)
                        continue;

                    const auto input = context.Widen(
                        context.Input(slot),
                        m_Inputs[slot].ValueType
                    );

                    result += "(" + input + ")";
                    i += token.size();
                    replaced = true;

                    break;
                }

                if (!replaced)
                    result += m_Expression[i++];
            }

            return { std::move(result), m_Type };
        }

    private:
        std::string m_Expression;
        EMaterialValueType m_Type;
        EShaderStage m_SupportedStages;
    };
}
